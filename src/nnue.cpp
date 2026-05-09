#include "nnue.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace SHAYVERI {
namespace NNUE {

I16 feature_weights[INPUT_SIZE][HIDDEN_SIZE];
I16 feature_bias[HIDDEN_SIZE];
I16 output_weights[HIDDEN_SIZE * 2];
I32 output_bias;

namespace {

static constexpr U32 NNUE_MAGIC   = 0x4E4E5545u;
static constexpr U32 NNUE_VERSION = 2u;

std::string g_net_path;
U64         g_net_hash = 0;
bool        g_loaded = false;

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

int piece_type_index(Piece p) {
    return static_cast<int>(get_type(p)) - 1;
}

int piece_colour_index(Piece p) {
    return static_cast<int>(get_colour(p));
}

I32 screlu(I16 x) {
    I32 clamped = std::clamp<I32>(x, 0, L1_SCALE);
    return clamped * clamped;
}

[[maybe_unused]] int evaluate_scalar(int side_to_move, const Accumulator &acc) {
    const I16 *white_acc = acc.vals[WHITE];
    const I16 *black_acc = acc.vals[BLACK];

    I64 sum = 0;
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        sum += static_cast<I64>(screlu(white_acc[i])) * output_weights[i];
        sum += static_cast<I64>(screlu(black_acc[i])) * output_weights[HIDDEN_SIZE + i];
    }

    sum /= L1_SCALE;
    sum += output_bias;
    int score = static_cast<int>(sum * OUTPUT_SCALE / (L1_SCALE * L1_SCALE));
    return side_to_move == WHITE ? score : -score;
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
    if (version != NNUE_VERSION) {
        std::fprintf(stderr, "[nnue] unsupported version %u in \"%s\"\n", version, path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }

    must_read(f, feature_weights, sizeof(feature_weights), "feature_weights");
    must_read(f, feature_bias, sizeof(feature_bias), "feature_bias");
    must_read(f, output_weights, sizeof(output_weights), "output_weights");
    must_read(f, &output_bias, sizeof(output_bias), "output_bias");

    U8 probe = 0;
    if (std::fread(&probe, 1, 1, f) != 0) {
        std::fprintf(stderr, "[nnue] \"%s\" is larger than expected\n", path.c_str());
        std::fclose(f);
        std::exit(EXIT_FAILURE);
    }
    std::fclose(f);

    U64 h = fnv1a_hash(feature_weights, sizeof(feature_weights));
    h ^= fnv1a_hash(feature_bias, sizeof(feature_bias));
    h ^= fnv1a_hash(output_weights, sizeof(output_weights));
    h ^= fnv1a_hash(&output_bias, sizeof(output_bias));

    g_net_path = path;
    g_net_hash = h;
    g_loaded = true;
    return path;
}

bool is_loaded() {
    return g_loaded;
}

void print_info() {
    std::cout << "info string NNUE path " << g_net_path << "\n"
              << "info string NNUE hash " << std::uppercase << std::hex
              << std::setw(16) << std::setfill('0')
              << static_cast<unsigned long long>(g_net_hash)
              << std::dec << std::nouppercase << std::setfill(' ') << "\n"
              << "info string NNUE arch Chess768 hidden=" << HIDDEN_SIZE << "\n"
              << "info string NNUE scales L1=" << L1_SCALE
              << " OUT=" << OUTPUT_SCALE << "\n";
}

void Accumulator::reset() {
    std::memset(vals, 0, sizeof(vals));
}

void Accumulator::refresh(const Board &board) {
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
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
            chess768_indices(piece_type_index(piece), piece_colour_index(piece),
                             sq, wi, bi);
            for (int i = 0; i < HIDDEN_SIZE; ++i) {
                vals[0][i] += feature_weights[wi][i];
                vals[1][i] += feature_weights[bi][i];
            }
        }
    }
}

void Accumulator::apply_delta(int add_white, int add_black,
                              int sub_white, int sub_black) {
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        vals[0][i] += feature_weights[add_white][i] - feature_weights[sub_white][i];
        vals[1][i] += feature_weights[add_black][i] - feature_weights[sub_black][i];
    }
}

void Accumulator::apply_deltas(const Delta *deltas, int count) {
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
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
