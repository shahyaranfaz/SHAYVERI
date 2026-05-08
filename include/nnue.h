#ifndef NNUE_H
#define NNUE_H

#include "board.h"
#include "types.h"

#include <string>

namespace SHAYVERI {
namespace NNUE {

inline constexpr int INPUT_SIZE   = 768;
inline constexpr int HIDDEN_SIZE  = 256;
inline constexpr int OUTPUT_SIZE  = 1;
inline constexpr int L1_SCALE     = 255;
inline constexpr int OUTPUT_SCALE = 400;

extern I16 feature_weights[INPUT_SIZE][HIDDEN_SIZE];
extern I16 feature_bias[HIDDEN_SIZE];
extern I16 output_weights[HIDDEN_SIZE * 2];
extern I32 output_bias;

struct Accumulator {
    struct Delta {
        int add_w;
        int add_b;
        int sub_w;
        int sub_b;
    };

    I16 vals[2][HIDDEN_SIZE]{};

    Accumulator() { reset(); }

    void reset();
    void refresh(const Board &board);
    void apply_delta(int add_white, int add_black, int sub_white, int sub_black);
    void apply_deltas(const Delta *deltas, int count);
};

inline int chess768_index(int piece_type, int piece_color, int sq, int perspective) {
    const int effective_sq = (perspective == 1) ? (sq ^ 56) : sq;
    return (piece_type * 2 + piece_color) * 64 + effective_sq;
}

inline void chess768_indices(int piece_type, int piece_color, int sq,
                             int &white_idx, int &black_idx) {
    white_idx = chess768_index(piece_type, piece_color, sq, 0);
    black_idx = chess768_index(piece_type, piece_color, sq, 1);
}

std::string load(const std::string &path);
bool is_loaded();
void print_info();

inline constexpr const char *UCI_OPTION_NAME = "EvalFile";

int evaluate(int side_to_move, const Accumulator &acc);

} // namespace NNUE
} // namespace SHAYVERI

#endif // NNUE_H
