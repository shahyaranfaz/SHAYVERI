#ifndef ATTACKS_H
#define ATTACKS_H

#include "board.h"

namespace SHAYVERI {

U64    pawn_attacks(Colour c, Square from);
U64    knight_attacks(Square from);
U64    bishop_attacks(Square from, U64 occupied);
U64    rook_attacks(Square from, U64 occupied);
U64    queen_attacks(Square from, U64 occupied);
U64    king_attacks(Square from);

bool   is_square_attacked(const Board &b, Square sq, Colour attacker);
Square king_square(const Board &b, Colour c);

void init_attacks();

} // namespace SHAYVERI

#endif // ATTACKS_H
