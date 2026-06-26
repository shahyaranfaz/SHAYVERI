#include "nnue.h"
#include "attacks.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace SHAYVERI {

namespace NNUE {

I16 feature_weights[MAX_INPUT_SIZE][MAX_HIDDEN_SIZE];
I16 feature_bias[MAX_HIDDEN_SIZE];
I16 output_weights[MAX_HIDDEN_SIZE * 2];
I32 output_bias;

namespace {

static constexpr U32 NNUE_MAGIC   = 0x4E4E5545u;
static constexpr U32 NNUE_VERSION_CLASSIC = 2u;
static constexpr U32 NNUE_VERSION_KB      = 3u;

std::string g_net_path;
U64         g_net_hash = 0;
bool        g_loaded = false;
bool        g_enabled = true;
int         g_king_buckets = 1;
int         g_input_size = CHESS768_INPUT_SIZE;
int         g_hidden_size = 256;
bool        g_use_screlu = false;

U64 fnv1a_hash(const void *data, size_t bytes) {
    const U8 *p = static_cast<const U8 *>(data);
    U64 h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < bytes; ++i)
        h = (h ^ p[i]) * 0x00000100000001b3ULL;
    return h;
}

void must_read(FILE *f, void *dst, size_t n, const char *what) {
    if (std::fread(dst, 1, n, f) != n) {
        std::fprintf(stderr, "[nnue] failed to read %s (%zu bytes)\n", what, n);
        std::exit(EXIT_FAILURE);
    }
}

long file_size(FILE *f) {
    const long cur = std::ftell(f);
    if (cur < 0 || std::fseek(f, 0, SEEK_END) != 0) {
        std::fprintf(stderr, "[nnue] failed to measure network file\n");
        std::exit(EXIT_FAILURE);
    }

    const long end = std::ftell(f);
    if (end < 0 || std::fseek(f, cur, SEEK_SET) != 0) {
        std::fprintf(stderr, "[nnue] failed to restore network file position\n");
        std::exit(EXIT_FAILURE);
    }

    return end;
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

[[maybe_unused]] int evaluate_scalar(int side_to_move, const Accumulator &acc) {
    const I16 *stm_acc = acc.vals[side_to_move];
    const I16 *nstm_acc = acc.vals[side_to_move ^ 1];

    I64 sum = 0;
    for (int i = 0; i < g_hidden_size; ++i) {
        if (g_use_screlu) {
            sum += static_cast<I64>(squared_crelu_scaled(stm_acc[i])) * output_weights[i];
            sum += static_cast<I64>(squared_crelu_scaled(nstm_acc[i])) * output_weights[g_hidden_size + i];
        } else {
            sum += static_cast<I64>(screlu(stm_acc[i])) * output_weights[i];
            sum += static_cast<I64>(screlu(nstm_acc[i])) * output_weights[g_hidden_size + i];
        }
    }

    sum += output_bias;
    int score = static_cast<int>(sum * OUTPUT_SCALE / (L1_SCALE * L1_SCALE));
    return score;
}

} // namespace

std::string load(const std::string &path) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr,
                     "[nnue] cannot open network file \"%s\"\n"
                     "[nnue] set the EvalFile UCI option to a valid .nnue path\n",
                     path.c_str());
        std::exit(EXIT_FAILURE);
    }

    U32 magic = 0;
    U32 version = 0;
    must_read(f, &magic, sizeof(magic), "magic");
    must_read(f, &version, sizeof(version), "version");

    if (magic != NNUE_MAGIC) {
        std::fprintf(stderr, "[nnue] bad magic 0x%08X in \"%s\"\n", magic, path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }
    if (version != NNUE_VERSION_CLASSIC && version != NNUE_VERSION_KB) {
        std::fprintf(stderr, "[nnue] unsupported version %u in \"%s\"\n", version, path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }

    if (version == NNUE_VERSION_CLASSIC) {
        g_king_buckets = 1;
        g_input_size = CHESS768_INPUT_SIZE;
        g_use_screlu = false;
    } else {
        U32 buckets = 0;
        must_read(f, &buckets, sizeof(buckets), "king_bucket_count");
        if (buckets != 8 && buckets != 16) {
            std::fprintf(stderr, "[nnue] invalid king bucket count %u in \"%s\"\n",
                         buckets, path.c_str());
            std::fclose(f);
            std::exit(EXIT_FAILURE);
        }
        g_king_buckets = static_cast<int>(buckets);
        g_input_size = CHESS768_INPUT_SIZE * g_king_buckets;
        g_use_screlu = true;
    }

    const long end = file_size(f);
    const long cur = std::ftell(f);
    if (cur < 0 || end < cur) {
        std::fprintf(stderr, "[nnue] failed to measure payload in \"%s\"\n", path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }

    const long payload_bytes = end - cur;
    const long hidden_stride_bytes = static_cast<long>(g_input_size * sizeof(I16) + sizeof(I16) + 2 * sizeof(I16));
    const long fixed_bytes = sizeof(output_bias);
    if (payload_bytes <= fixed_bytes || (payload_bytes - fixed_bytes) % hidden_stride_bytes != 0) {
        std::fprintf(stderr,
                     "[nnue] cannot infer hidden size from payload %ld bytes in \"%s\"\n",
                     payload_bytes, path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }

    const long inferred_hidden = (payload_bytes - fixed_bytes) / hidden_stride_bytes;
    if (inferred_hidden != 256 && inferred_hidden != 512) {
        std::fprintf(stderr,
                     "[nnue] unsupported hidden size %ld in \"%s\"; supported sizes are 256 and 512\n",
                     inferred_hidden, path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }
    if (inferred_hidden > MAX_HIDDEN_SIZE) {
        std::fprintf(stderr,
                     "[nnue] hidden size %ld exceeds compiled max %d in \"%s\"\n",
                     inferred_hidden, MAX_HIDDEN_SIZE, path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }
    g_hidden_size = static_cast<int>(inferred_hidden);

    std::memset(feature_weights, 0, sizeof(feature_weights));
    std::memset(feature_bias, 0, sizeof(feature_bias));
    std::memset(output_weights, 0, sizeof(output_weights));

    const size_t feature_weights_row_bytes =
        static_cast<size_t>(g_hidden_size) * sizeof(I16);
    for (int input = 0; input < g_input_size; ++input)
        must_read(f, feature_weights[input], feature_weights_row_bytes, "feature_weights");

    must_read(f, feature_bias, static_cast<size_t>(g_hidden_size) * sizeof(I16), "feature_bias");
    must_read(f, output_weights, static_cast<size_t>(g_hidden_size) * 2 * sizeof(I16), "output_weights");
    must_read(f, &output_bias, sizeof(output_bias), "output_bias");

    U8 probe = 0;
    if (std::fread(&probe, 1, 1, f) != 0) {
        std::fprintf(stderr, "[nnue] \"%s\" is larger than expected\n", path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }
    std::fclose(f);

    U64 h = 0xcbf29ce484222325ULL;
    for (int input = 0; input < g_input_size; ++input)
        h ^= fnv1a_hash(feature_weights[input], feature_weights_row_bytes);
    h ^= fnv1a_hash(feature_bias, static_cast<size_t>(g_hidden_size) * sizeof(I16));
    h ^= fnv1a_hash(output_weights, static_cast<size_t>(g_hidden_size) * 2 * sizeof(I16));
    h ^= fnv1a_hash(&output_bias, sizeof(output_bias));
    h ^= fnv1a_hash(&g_king_buckets, sizeof(g_king_buckets));
    h ^= fnv1a_hash(&g_hidden_size, sizeof(g_hidden_size));
    h ^= fnv1a_hash(&g_use_screlu, sizeof(g_use_screlu));

    g_net_path = path;
    g_net_hash = h;
    g_loaded = true;
    return path;
}

bool is_loaded() {
    return g_loaded;
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

int active_input_size() {
    return g_input_size;
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
}

void Accumulator::apply_delta(int add_white, int add_black,
                              int sub_white, int sub_black) {
    for (int i = 0; i < g_hidden_size; ++i) {
        vals[0][i] += feature_weights[add_white][i] - feature_weights[sub_white][i];
        vals[1][i] += feature_weights[add_black][i] - feature_weights[sub_black][i];
    }
}

void Accumulator::apply_deltas(const Delta *deltas, int count) {
    for (int i = 0; i < g_hidden_size; ++i) {
        int dw = 0;
        int db = 0;
        for (int k = 0; k < count; ++k) {
            dw += feature_weights[deltas[k].add_w][i] - feature_weights[deltas[k].sub_w][i];
            db += feature_weights[deltas[k].add_b][i] - feature_weights[deltas[k].sub_b][i];
        }
        vals[0][i] += static_cast<I16>(dw);
        vals[1][i] += static_cast<I16>(db);
    }
}

#ifdef __AVX2__
int evaluate_avx2(int side_to_move, const Accumulator &acc);
#endif

int evaluate(int side_to_move, const Accumulator &acc) {
#ifdef __AVX2__
    return evaluate_avx2(side_to_move, acc);
#else
    return evaluate_scalar(side_to_move, acc);
#endif
}

} // namespace NNUE

} // namespace SHAYVERI
