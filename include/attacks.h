#ifndef ATTACKS_H
#define ATTACKS_H

#include "board.h"

namespace SHAYVERI {

// Precomputed lookup tables — populated by init_attacks().
extern U64 PAWN_ATTACKS[2][64];
extern U64 KNIGHT_ATTACKS[64];
extern U64 KING_ATTACKS[64];

// Inline wrappers: single array lookup, zero function-call overhead.
inline U64 pawn_attacks(Colour c, Square from)  { return PAWN_ATTACKS[int(c)][from]; }
inline U64 knight_attacks(Square from)          { return KNIGHT_ATTACKS[from]; }
inline U64 king_attacks(Square from)            { return KING_ATTACKS[from]; }

U64 bishop_attacks(Square from, U64 occupied);
U64 rook_attacks(Square from, U64 occupied);
U64 queen_attacks(Square from, U64 occupied);

bool is_square_attacked(const Board &b, Square sq, Colour attacker);
Square king_square(const Board &b, Colour c);

void init_attacks();

} // namespace SHAYVERI

#endif // ATTACKS_H
