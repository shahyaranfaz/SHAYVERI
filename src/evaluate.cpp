#include "evaluate.h"
#include "types.h"

static constexpr int PIECE_VALUES[PIECE_COUNT] = {
    0,        // NONE_PIECE
    100,      // WP
    320,      // WN
    330,      // WB
    500,      // WR
    900,      // WQ
    0,        // WK
    -100,     // BP
    -320,     // BN
    -330,     // BB
    -500,     // BR
    -900,     // BQ
    0,        // BK
};

int evaluate(Board &b) {
    int score = 0;
    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = b.bit_boards[p];
        while (bb) {
            bb &= bb - 1;
            score += PIECE_VALUES[p];
        }
    }
    return b.side_to_move == WHITE ? score : -score;
}