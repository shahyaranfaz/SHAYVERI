#ifndef MOVE_H
#define MOVE_H

#include "types.h"

// Move encoding:
//   bits  0-5  : from square
//   bits  6-11 : to square
//   bits 12-15 : promotion piece type (0=none, 1=N, 2=B, 3=R, 4=Q)

namespace SHAYVERI {

using Move = U32;

constexpr Move MOVE_NONE = 0;

constexpr Move create_move(Square from, Square to, PieceType promo = NONE_PTYPE) {
    return (Move(from) & 63u) | ((Move(to) & 63u) << 6) | ((Move(promo) & 15u) << 12);
}

constexpr Square    move_from(Move m)  { return Square(m & 63u); }
constexpr Square    move_to(Move m)    { return Square((m >> 6) & 63u); }
constexpr PieceType move_promo(Move m) { return PieceType((m >> 12) & 15u); }
constexpr bool      is_promo(Move m)   { return move_promo(m) != NONE_PTYPE; }

} // namespace SHAYVERI

#endif // MOVE_H
