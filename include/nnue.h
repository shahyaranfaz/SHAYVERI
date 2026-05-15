#ifndef NNUE_H
#define NNUE_H

#include "board.h"
#include "types.h"

#include <string>

namespace SHAYVERI {

namespace NNUE {

inline constexpr int CHESS768_INPUT_SIZE = 768;
inline constexpr int MAX_KING_BUCKETS    = 8;
inline constexpr int MAX_INPUT_SIZE      = CHESS768_INPUT_SIZE * MAX_KING_BUCKETS;
inline constexpr int HIDDEN_SIZE         = 256;
inline constexpr int OUTPUT_SIZE         = 1;
inline constexpr int L1_SCALE            = 255;
inline constexpr int OUTPUT_SCALE        = 400;

extern I16 feature_weights[MAX_INPUT_SIZE][HIDDEN_SIZE];
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
    const int effective_color = (perspective == 1) ? (piece_color ^ 1) : piece_color;
    return ((effective_color * 6) + piece_type) * 64 + effective_sq;
}

int king_bucket_count();
int active_input_size();
bool has_king_buckets();

inline int king_bucket_index(int king_sq, int perspective, int bucket_count) {
    if (bucket_count <= 1) return 0;

    const int effective_sq = (perspective == 1) ? (king_sq ^ 56) : king_sq;
    const int file = effective_sq & 7;
    const int rank = effective_sq >> 3;
    static constexpr int FILE_MAP[8] = {0, 1, 2, 3, 3, 2, 1, 0};
    return (rank >= 4 ? 1 : 0) * 4 + FILE_MAP[file];
}

inline int feature_index(int piece_type, int piece_color, int sq, int perspective, int perspective_king_sq) {
    if (king_bucket_count() <= 1)
        return chess768_index(piece_type, piece_color, sq, perspective);

    const int king_file = perspective_king_sq & 7; // file unaffected by ^56
    const int flip      = (king_file > 3) ? 7 : 0;
    const int base      = chess768_index(piece_type, piece_color, sq, perspective);
    const int bucket    = king_bucket_index(perspective_king_sq, perspective, king_bucket_count());
    return bucket * CHESS768_INPUT_SIZE + (base ^ flip);
}

inline void chess768_indices(int piece_type, int piece_color, int sq,
                             int &white_idx, int &black_idx) {
    white_idx = chess768_index(piece_type, piece_color, sq, 0);
    black_idx = chess768_index(piece_type, piece_color, sq, 1);
}

inline void feature_indices(int piece_type, int piece_color, int sq,
                            int white_king_sq, int black_king_sq,
                            int &white_idx, int &black_idx) {
    white_idx = feature_index(piece_type, piece_color, sq, 0, white_king_sq);
    black_idx = feature_index(piece_type, piece_color, sq, 1, black_king_sq);
}

std::string load(const std::string &path);
bool is_loaded();
void print_info();

inline constexpr const char *UCI_OPTION_NAME = "EvalFile";

int evaluate(int side_to_move, const Accumulator &acc);

} // namespace NNUE

} // namespace SHAYVERI

#endif // NNUE_H
