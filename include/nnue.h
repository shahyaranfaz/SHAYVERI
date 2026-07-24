#ifndef NNUE_H
#define NNUE_H

#include "board.h"
#include "types.h"

#include <algorithm>
#include <cstddef>
#include <string>

namespace SHAYVERI {

namespace NNUE {

inline constexpr int CHESS768_INPUT_SIZE = 768;
inline constexpr int MAX_KING_BUCKETS    = 16;
inline constexpr int MAX_INPUT_SIZE      = CHESS768_INPUT_SIZE * MAX_KING_BUCKETS;
inline constexpr int MAX_HIDDEN_SIZE     = 512;
inline constexpr int MAX_OUTPUT_BUCKETS  = 8;
inline constexpr int L1_SCALE            = 255;
inline constexpr int OUTPUT_SCALE        = 400;

extern I16 feature_weights[MAX_INPUT_SIZE][MAX_HIDDEN_SIZE];
extern I16 feature_bias[MAX_HIDDEN_SIZE];
extern I16 output_weights[MAX_OUTPUT_BUCKETS][MAX_HIDDEN_SIZE * 2];
extern I32 output_bias[MAX_OUTPUT_BUCKETS];

extern const char EMBEDDED_DEFAULT_NET_NAME[];
extern const U8 EMBEDDED_DEFAULT_NET[];
extern const std::size_t EMBEDDED_DEFAULT_NET_SIZE;

struct Accumulator {
    I16 vals[2][MAX_HIDDEN_SIZE]{};

    Accumulator() { reset(); }

    void reset();
    void refresh(const Board &board);
    void refresh_perspective(const Board &board, int perspective);
};

inline int chess768_index(int piece_type, int piece_colour, int sq, int perspective) {
    const int effective_sq = (perspective == 1) ? (sq ^ 56) : sq;
    const int effective_colour = (perspective == 1) ? (piece_colour ^ 1) : piece_colour;
    return ((effective_colour * 6) + piece_type) * 64 + effective_sq;
}

int king_bucket_count();
int active_hidden_size();
bool has_king_buckets();
bool uses_screlu();
int output_bucket_count();

inline int material_bucket(int piece_count) {
    return std::clamp((piece_count - 1) / 4, 0, MAX_OUTPUT_BUCKETS - 1);
}

inline int king_bucket_index(int king_sq, int perspective, int bucket_count) {
    if (bucket_count <= 1) return 0;

    const int effective_sq = (perspective == 1) ? (king_sq ^ 56) : king_sq;
    const int file = effective_sq & 7;
    const int rank = effective_sq >> 3;
    static constexpr int FILE_MAP[8] = {0, 1, 2, 3, 3, 2, 1, 0};
    const int file_bucket = FILE_MAP[file];
    if (bucket_count == 8)
        return (rank >= 4 ? 1 : 0) * 4 + file_bucket;
    if (bucket_count == 16)
        return (rank / 2) * 4 + file_bucket;

    return 0;
}

inline int feature_index(int piece_type, int piece_colour, int sq, int perspective, int perspective_king_sq) {
    if (king_bucket_count() <= 1)
        return chess768_index(piece_type, piece_colour, sq, perspective);

    const int king_file = perspective_king_sq & 7; // file unaffected by ^56
    const int flip      = (king_file > 3) ? 7 : 0;
    const int base      = chess768_index(piece_type, piece_colour, sq, perspective);
    const int bucket    = king_bucket_index(perspective_king_sq, perspective, king_bucket_count());
    return bucket * CHESS768_INPUT_SIZE + (base ^ flip);
}

inline void feature_indices(int piece_type, int piece_colour, int sq,
                            int white_king_sq, int black_king_sq,
                            int &white_idx, int &black_idx) {
    white_idx = feature_index(piece_type, piece_colour, sq, 0, white_king_sq);
    black_idx = feature_index(piece_type, piece_colour, sq, 1, black_king_sq);
}

bool load(const std::string &path, std::string &error);
bool load_embedded_default(std::string &error);
bool is_enabled();
void set_enabled(bool enabled);
void print_info();

inline constexpr const char *UCI_OPTION_NAME = "EvalFile";

int evaluate(int side_to_move, int piece_count, const Accumulator &acc);

} // namespace NNUE

} // namespace SHAYVERI

#endif // NNUE_H
