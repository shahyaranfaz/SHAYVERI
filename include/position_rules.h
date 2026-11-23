#ifndef POSITION_RULES_H
#define POSITION_RULES_H

#include "attacks.h"
#include "board.h"

#include <span>

namespace SHAYVERI::PositionRules {

inline bool is_in_check(const Board &b) {
    const Square king = king_square(b, b.side_to_move);
    return is_square_attacked(b, king, flip(b.side_to_move));
}

inline bool has_insufficient_material(const Board &b) {
    if (b.bit_boards[WP] | b.bit_boards[BP]
        | b.bit_boards[WR] | b.bit_boards[BR]
        | b.bit_boards[WQ] | b.bit_boards[BQ])
        return false;

    const U64 knights = b.bit_boards[WN] | b.bit_boards[BN];
    U64 bishops = b.bit_boards[WB] | b.bit_boards[BB];
    if (__builtin_popcountll(knights | bishops) <= 1)
        return true;
    if (knights != 0 || bishops == 0)
        return false;

    const Square first = static_cast<Square>(__builtin_ctzll(bishops));
    const int square_colour = (static_cast<int>(get_file(first))
        + static_cast<int>(get_rank(first))) & 1;
    while (bishops) {
        const Square square = pop_lsb(bishops);
        const int colour = (static_cast<int>(get_file(square))
            + static_cast<int>(get_rank(square))) & 1;
        if (colour != square_colour)
            return false;
    }
    return true;
}

// Search treats a prior occurrence as a draw. Only same-side positions inside
// the reversible-move window can repeat.
inline bool has_search_repetition(
    U64 key, const U64 *history, int length, int half_move) {
    int limit = length - half_move;
    if (limit < 0) limit = 0;
    for (int i = length - 2; i >= limit; i -= 2) {
        if (history[i] == key)
            return true;
    }
    return false;
}

// Played games claim repetition only on the third occurrence.
inline bool is_threefold_repetition(std::span<const U64> history, U64 key) {
    int seen = 0;
    for (const U64 prior : history) {
        if (prior == key && ++seen >= 3)
            return true;
    }
    return false;
}

} // namespace SHAYVERI::PositionRules

#endif // POSITION_RULES_H
