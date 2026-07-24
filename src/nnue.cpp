#include "nnue.h"

#include "attacks.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace SHAYVERI {

namespace NNUE {

I16 feature_weights[MAX_INPUT_SIZE][MAX_HIDDEN_SIZE];
I16 feature_bias[MAX_HIDDEN_SIZE];
I16 output_weights[MAX_OUTPUT_BUCKETS][MAX_HIDDEN_SIZE * 2];
I32 output_bias[MAX_OUTPUT_BUCKETS];

namespace {

static constexpr U32 NNUE_MAGIC   = 0x4E4E5545u;
static constexpr U32 NNUE_VERSION_CLASSIC = 2u;
static constexpr U32 NNUE_VERSION_KB      = 3u;
static constexpr U32 NNUE_VERSION_BUCKETED = 4u;
static constexpr U32 NNUE_FLAG_SCRELU = 1u;

std::string g_net_path;
U64         g_net_hash = 0;
bool        g_loaded = false;
bool        g_enabled = true;
int         g_king_buckets = 1;
int         g_hidden_size = 256;
bool        g_use_screlu = false;
int         g_output_buckets = 1;

U64 fnv1a_hash(const void *data, size_t bytes) {
    const U8 *p = static_cast<const U8 *>(data);
    U64 h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < bytes; ++i)
        h = (h ^ p[i]) * 0x00000100000001b3ULL;
    return h;
}

bool read_bytes(const U8 *data, size_t size, size_t &offset, void *dst, size_t n,
                const char *what, const std::string &label, std::string &error) {
    if (offset > size || n > size - offset) {
        error = "failed to read " + std::string(what) + " (" +
                std::to_string(n) + " bytes) in \"" + label + "\"";
        return false;
    }
    std::memcpy(dst, data + offset, n);
    offset += n;
    return true;
}

bool read_file_bytes(const std::string &path, std::vector<U8> &bytes,
                     std::string &error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open network file \"" + path + "\"";
        return false;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff end = in.tellg();
    if (end <= 0) {
        error = "empty or unreadable network file \"" + path + "\"";
        return false;
    }
    in.seekg(0, std::ios::beg);

    bytes.resize(static_cast<size_t>(end));
    if (!in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(end))) {
        error = "failed to read network file \"" + path + "\"";
        return false;
    }
    return true;
}

int piece_type_index(Piece p) {
    return static_cast<int>(get_type(p)) - 1;
}

int piece_colour_index(Piece p) {
    return static_cast<int>(get_colour(p));
}

I32 screlu(I16 x) {
    return std::clamp<I32>(x, 0, L1_SCALE);
}

I32 squared_crelu_scaled(I16 x) {
    const I32 y = std::clamp<I32>(x, 0, L1_SCALE);
    return (y * y) / L1_SCALE;
}

[[maybe_unused]] int evaluate_scalar(
    int side_to_move, int piece_count, const Accumulator &acc) {
    const I16 *stm_acc = acc.vals[side_to_move];
    const I16 *nstm_acc = acc.vals[side_to_move ^ 1];
    const int bucket = g_output_buckets == 1 ? 0 : material_bucket(piece_count);
    const I16 *weights = output_weights[bucket];

    I64 sum = 0;
    for (int i = 0; i < g_hidden_size; ++i) {
        if (g_use_screlu) {
            sum += static_cast<I64>(squared_crelu_scaled(stm_acc[i])) * weights[i];
            sum += static_cast<I64>(squared_crelu_scaled(nstm_acc[i])) * weights[g_hidden_size + i];
        } else {
            sum += static_cast<I64>(screlu(stm_acc[i])) * weights[i];
            sum += static_cast<I64>(screlu(nstm_acc[i])) * weights[g_hidden_size + i];
        }
    }

    sum += output_bias[bucket];
    int score = static_cast<int>(sum * OUTPUT_SCALE / (L1_SCALE * L1_SCALE));
    return score;
}

struct PendingNetwork {
    int king_buckets = 1;
    int input_size = CHESS768_INPUT_SIZE;
    int hidden_size = 256;
    bool use_screlu = false;
    int output_buckets = 1;
    std::vector<I16> feature_weights;
    std::vector<I16> feature_bias;
    std::vector<I16> output_weights;
    std::vector<I32> output_bias;
};

bool load_from_bytes(const U8 *data, size_t size, const std::string &label,
                     std::string &error) {
    size_t offset = 0;
    U32 magic = 0;
    U32 version = 0;
    if (!read_bytes(data, size, offset, &magic, sizeof(magic), "magic", label, error) ||
        !read_bytes(data, size, offset, &version, sizeof(version), "version", label, error))
        return false;

    if (magic != NNUE_MAGIC) {
        error = "bad magic in \"" + label + "\"";
        return false;
    }
    if (version != NNUE_VERSION_CLASSIC && version != NNUE_VERSION_KB &&
        version != NNUE_VERSION_BUCKETED) {
        error = "unsupported version " + std::to_string(version) + " in \"" + label + "\"";
        return false;
    }

    PendingNetwork pending;
    size_t declared_feature_weights_bytes = 0;
    size_t declared_feature_bias_bytes = 0;
    size_t declared_output_weights_bytes = 0;
    size_t declared_output_bias_bytes = 0;

    if (version == NNUE_VERSION_CLASSIC) {
        pending.king_buckets = 1;
        pending.input_size = CHESS768_INPUT_SIZE;
        pending.use_screlu = false;
    } else if (version == NNUE_VERSION_KB) {
        U32 buckets = 0;
        if (!read_bytes(data, size, offset, &buckets, sizeof(buckets),
                        "king_bucket_count", label, error))
            return false;
        if (buckets != 8 && buckets != 16) {
            error = "invalid king bucket count " + std::to_string(buckets) +
                    " in \"" + label + "\"";
            return false;
        }
        pending.king_buckets = static_cast<int>(buckets);
        pending.input_size = CHESS768_INPUT_SIZE * pending.king_buckets;
        pending.use_screlu = true;
    } else {
        U32 buckets = 0;
        U32 hidden = 0;
        U32 output_buckets = 0;
        U32 flags = 0;
        U32 feature_weights_bytes = 0;
        U32 feature_bias_bytes = 0;
        U32 output_weights_bytes = 0;
        U32 output_bias_bytes = 0;
        if (!read_bytes(data, size, offset, &buckets, sizeof(buckets),
                        "king_bucket_count", label, error) ||
            !read_bytes(data, size, offset, &hidden, sizeof(hidden),
                        "hidden_size", label, error) ||
            !read_bytes(data, size, offset, &output_buckets, sizeof(output_buckets),
                        "output_bucket_count", label, error) ||
            !read_bytes(data, size, offset, &flags, sizeof(flags),
                        "flags", label, error) ||
            !read_bytes(data, size, offset, &feature_weights_bytes, sizeof(feature_weights_bytes),
                        "feature_weights_bytes", label, error) ||
            !read_bytes(data, size, offset, &feature_bias_bytes, sizeof(feature_bias_bytes),
                        "feature_bias_bytes", label, error) ||
            !read_bytes(data, size, offset, &output_weights_bytes, sizeof(output_weights_bytes),
                        "output_weights_bytes", label, error) ||
            !read_bytes(data, size, offset, &output_bias_bytes, sizeof(output_bias_bytes),
                        "output_bias_bytes", label, error))
            return false;

        if (buckets != 16 || hidden != 512 ||
            output_buckets != MAX_OUTPUT_BUCKETS || flags != NNUE_FLAG_SCRELU) {
            error = "unsupported v4 architecture in \"" + label + "\"";
            return false;
        }
        pending.king_buckets = static_cast<int>(buckets);
        pending.input_size = CHESS768_INPUT_SIZE * pending.king_buckets;
        pending.hidden_size = static_cast<int>(hidden);
        pending.output_buckets = static_cast<int>(output_buckets);
        pending.use_screlu = true;
        declared_feature_weights_bytes = feature_weights_bytes;
        declared_feature_bias_bytes = feature_bias_bytes;
        declared_output_weights_bytes = output_weights_bytes;
        declared_output_bias_bytes = output_bias_bytes;
    }

    const size_t payload_bytes = size - offset;
    size_t inferred_hidden = static_cast<size_t>(pending.hidden_size);
    if (version != NNUE_VERSION_BUCKETED) {
        const size_t hidden_stride_bytes =
            static_cast<size_t>(pending.input_size) * sizeof(I16) + 3 * sizeof(I16);
        const size_t fixed_bytes = sizeof(I32);
        if (payload_bytes <= fixed_bytes ||
            (payload_bytes - fixed_bytes) % hidden_stride_bytes != 0) {
            error = "cannot infer hidden size from payload " +
                    std::to_string(payload_bytes) + " bytes in \"" + label + "\"";
            return false;
        }
        inferred_hidden = (payload_bytes - fixed_bytes) / hidden_stride_bytes;
        if (inferred_hidden != 256 && inferred_hidden != 512) {
            error = "unsupported hidden size " + std::to_string(inferred_hidden) +
                    " in \"" + label + "\"; supported sizes are 256 and 512";
            return false;
        }
        pending.hidden_size = static_cast<int>(inferred_hidden);
    }

    const size_t weight_count = static_cast<size_t>(pending.input_size) * inferred_hidden;
    const size_t output_weight_count =
        inferred_hidden * 2 * static_cast<size_t>(pending.output_buckets);
    const size_t expected_feature_weights_bytes = weight_count * sizeof(I16);
    const size_t expected_feature_bias_bytes = inferred_hidden * sizeof(I16);
    const size_t expected_output_weights_bytes = output_weight_count * sizeof(I16);
    const size_t expected_output_bias_bytes =
        static_cast<size_t>(pending.output_buckets) * sizeof(I32);
    if (version == NNUE_VERSION_BUCKETED &&
        (declared_feature_weights_bytes != expected_feature_weights_bytes ||
         declared_feature_bias_bytes != expected_feature_bias_bytes ||
         declared_output_weights_bytes != expected_output_weights_bytes ||
         declared_output_bias_bytes != expected_output_bias_bytes ||
         payload_bytes != expected_feature_weights_bytes + expected_feature_bias_bytes +
                          expected_output_weights_bytes + expected_output_bias_bytes)) {
        error = "invalid v4 section lengths in \"" + label + "\"";
        return false;
    }

    pending.feature_weights.resize(weight_count);
    pending.feature_bias.resize(inferred_hidden);
    pending.output_weights.resize(output_weight_count);
    pending.output_bias.resize(pending.output_buckets);

    const size_t feature_weights_row_bytes =
        inferred_hidden * sizeof(I16);
    if (!read_bytes(data, size, offset, pending.feature_weights.data(),
                    weight_count * sizeof(I16), "feature_weights", label, error) ||
        !read_bytes(data, size, offset, pending.feature_bias.data(),
                    inferred_hidden * sizeof(I16), "feature_bias", label, error) ||
        !read_bytes(data, size, offset, pending.output_weights.data(),
                    output_weight_count * sizeof(I16), "output_weights", label, error) ||
        !read_bytes(data, size, offset, pending.output_bias.data(),
                    pending.output_bias.size() * sizeof(I32), "output_bias", label, error))
        return false;

    if (offset != size) {
        error = "\"" + label + "\" is larger than expected";
        return false;
    }

    U64 h = 0xcbf29ce484222325ULL;
    for (int input = 0; input < pending.input_size; ++input) {
        const size_t row = static_cast<size_t>(input) * inferred_hidden;
        h ^= fnv1a_hash(pending.feature_weights.data() + row,
                        feature_weights_row_bytes);
    }
    h ^= fnv1a_hash(pending.feature_bias.data(), inferred_hidden * sizeof(I16));
    h ^= fnv1a_hash(pending.output_weights.data(), output_weight_count * sizeof(I16));
    h ^= fnv1a_hash(pending.output_bias.data(),
                    pending.output_bias.size() * sizeof(I32));
    h ^= fnv1a_hash(&pending.king_buckets, sizeof(pending.king_buckets));
    h ^= fnv1a_hash(&pending.hidden_size, sizeof(pending.hidden_size));
    h ^= fnv1a_hash(&pending.use_screlu, sizeof(pending.use_screlu));
    h ^= fnv1a_hash(&pending.output_buckets, sizeof(pending.output_buckets));

    // Complete the final potentially-throwing allocation before changing any
    // live evaluator state. The commit below is then entirely non-throwing.
    std::string committed_path = label;

    std::memset(feature_weights, 0, sizeof(feature_weights));
    std::memset(feature_bias, 0, sizeof(feature_bias));
    std::memset(output_weights, 0, sizeof(output_weights));
    for (int input = 0; input < pending.input_size; ++input) {
        const size_t row = static_cast<size_t>(input) * inferred_hidden;
        std::memcpy(feature_weights[input], pending.feature_weights.data() + row,
                    feature_weights_row_bytes);
    }
    std::memcpy(feature_bias, pending.feature_bias.data(),
                inferred_hidden * sizeof(I16));
    std::memcpy(output_weights, pending.output_weights.data(),
                output_weight_count * sizeof(I16));
    std::memcpy(output_bias, pending.output_bias.data(),
                pending.output_bias.size() * sizeof(I32));

    g_net_path.swap(committed_path);
    g_net_hash = h;
    g_king_buckets = pending.king_buckets;
    g_hidden_size = pending.hidden_size;
    g_use_screlu = pending.use_screlu;
    g_output_buckets = pending.output_buckets;
    g_loaded = true;
    error.clear();
    return true;
}

} // namespace

bool load(const std::string &path, std::string &error) {
    try {
        std::vector<U8> bytes;
        if (!read_file_bytes(path, bytes, error)) return false;
        return load_from_bytes(bytes.data(), bytes.size(), path, error);
    } catch (const std::exception &e) {
        error = "failed to load network \"" + path + "\": " + e.what();
        return false;
    }
}

bool load_embedded_default(std::string &error) {
    try {
        return load_from_bytes(EMBEDDED_DEFAULT_NET, EMBEDDED_DEFAULT_NET_SIZE,
                               std::string("<embedded:") + EMBEDDED_DEFAULT_NET_NAME + ">",
                               error);
    } catch (const std::exception &e) {
        error = "failed to load embedded network: " + std::string(e.what());
        return false;
    }
}

bool is_enabled() {
    return g_enabled && g_loaded;
}

void set_enabled(bool enabled) {
    g_enabled = enabled;
}

int king_bucket_count() {
    return g_king_buckets;
}

int active_hidden_size() {
    return g_hidden_size;
}

bool has_king_buckets() {
    return g_king_buckets > 1;
}

bool uses_screlu() {
    return g_use_screlu;
}

int output_bucket_count() {
    return g_output_buckets;
}

void print_info() {
    std::cout << "info string NNUE path " << g_net_path << "\n"
              << "info string NNUE hash " << std::uppercase << std::hex
              << std::setw(16) << std::setfill('0')
              << static_cast<unsigned long long>(g_net_hash)
              << std::dec << std::nouppercase << std::setfill(' ') << "\n"
              << "info string NNUE arch "
              << (has_king_buckets() ? "Chess768xKingBuckets" : "Chess768")
              << " hidden=" << g_hidden_size
              << " king_buckets=" << g_king_buckets
              << " output_buckets=" << g_output_buckets
              << " activation=" << (g_use_screlu ? "SCReLU" : "CReLU") << "\n"
              << "info string NNUE scales L1=" << L1_SCALE
              << " OUT=" << OUTPUT_SCALE << "\n";
}

void Accumulator::reset() {
    std::memset(vals, 0, sizeof(vals));
}

void Accumulator::refresh(const Board &board) {
    const Square white_king_sq = king_square(board, WHITE);
    const Square black_king_sq = king_square(board, BLACK);

#ifdef __AVX2__
    // A legal chess position contains at most 32 pieces, but keep refresh
    // memory-safe for every board representation the parser can construct.
    int white_indices[64];
    int black_indices[64];
    int feature_count = 0;

    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = board.bit_boards[p];
        while (bb) {
            const Square sq = pop_lsb(bb);
            const Piece piece = Piece(p);
            feature_indices(piece_type_index(piece), piece_colour_index(piece), sq,
                            white_king_sq, black_king_sq,
                            white_indices[feature_count], black_indices[feature_count]);
            ++feature_count;
        }
    }

    constexpr int LANES = sizeof(__m256i) / sizeof(I16);
    for (int i = 0; i < g_hidden_size; i += LANES) {
        __m256i white = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(feature_bias + i));
        __m256i black = white;
        for (int f = 0; f < feature_count; ++f) {
            white = _mm256_add_epi16(white, _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(feature_weights[white_indices[f]] + i)));
            black = _mm256_add_epi16(black, _mm256_loadu_si256(
                reinterpret_cast<const __m256i *>(feature_weights[black_indices[f]] + i)));
        }
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(vals[0] + i), white);
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(vals[1] + i), black);
    }
#else
    for (int i = 0; i < g_hidden_size; ++i) {
        vals[0][i] = feature_bias[i];
        vals[1][i] = feature_bias[i];
    }

    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = board.bit_boards[p];
        while (bb) {
            Square sq = pop_lsb(bb);
            int wi = 0;
            int bi = 0;
            Piece piece = Piece(p);
            feature_indices(piece_type_index(piece), piece_colour_index(piece), sq,
                            white_king_sq, black_king_sq, wi, bi);
            for (int i = 0; i < g_hidden_size; ++i) {
                vals[0][i] += feature_weights[wi][i];
                vals[1][i] += feature_weights[bi][i];
            }
        }
    }
#endif
}

void Accumulator::refresh_perspective(const Board &board, int perspective) {
    const Square perspective_king_sq = king_square(
        board, static_cast<Colour>(perspective));

#ifdef __AVX2__
    int indices[64];
    int feature_count = 0;
    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = board.bit_boards[p];
        while (bb) {
            const Square sq = pop_lsb(bb);
            const Piece piece = Piece(p);
            indices[feature_count++] = feature_index(
                piece_type_index(piece), piece_colour_index(piece), sq,
                perspective, perspective_king_sq);
        }
    }

    constexpr int LANES = sizeof(__m256i) / sizeof(I16);
    for (int i = 0; i < g_hidden_size; i += LANES) {
        __m256i value = _mm256_loadu_si256(
            reinterpret_cast<const __m256i *>(feature_bias + i));
        for (int feature = 0; feature < feature_count; ++feature) {
            value = _mm256_add_epi16(
                value, _mm256_loadu_si256(
                    reinterpret_cast<const __m256i *>(
                        feature_weights[indices[feature]] + i)));
        }
        _mm256_storeu_si256(
            reinterpret_cast<__m256i *>(vals[perspective] + i), value);
    }
#else
    for (int i = 0; i < g_hidden_size; ++i)
        vals[perspective][i] = feature_bias[i];

    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = board.bit_boards[p];
        while (bb) {
            const Square sq = pop_lsb(bb);
            const Piece piece = Piece(p);
            const int index = feature_index(
                piece_type_index(piece), piece_colour_index(piece), sq,
                perspective, perspective_king_sq);
            for (int i = 0; i < g_hidden_size; ++i)
                vals[perspective][i] += feature_weights[index][i];
        }
    }
#endif
}

#ifdef __AVX2__
int evaluate_avx2(int side_to_move, int piece_count, const Accumulator &acc);
#endif

int evaluate(int side_to_move, int piece_count, const Accumulator &acc) {
#ifdef __AVX2__
    return evaluate_avx2(side_to_move, piece_count, acc);
#else
    return evaluate_scalar(side_to_move, piece_count, acc);
#endif
}

} // namespace NNUE

} // namespace SHAYVERI
