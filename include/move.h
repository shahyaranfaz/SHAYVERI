#ifndef MOVE_H
#define MOVE_H

#include "types.h"

// Move encoding:
//   bits  0-5  : from square
//   bits  6-11 : to square
//   bits 12-15 : special move (0=none, 1=en passant, 2=N, 3=B, 4=R, 5=Q)

namespace SHAYVERI {

using Move = U16;

constexpr Move MOVE_NONE = 0;

constexpr Move create_move(
    Square from, Square to, PieceType promo = NONE_PTYPE) {
    const Move special =
        promo >= KNIGHT && promo <= QUEEN ? static_cast<Move>(promo) : 0;
    return static_cast<Move>(
        (static_cast<Move>(from) & 63u)
        | ((static_cast<Move>(to) & 63u) << 6)
        | (special << 12));
}

// PieceType value 1 is unused by promotions and denotes en passant.
constexpr Move create_ep_move(Square from, Square to) {
    return static_cast<Move>(
        (static_cast<Move>(from) & 63u)
        | ((static_cast<Move>(to) & 63u) << 6)
        | (static_cast<Move>(PAWN) << 12));
}

constexpr Square    move_from(Move m) { return Square(m & 63u); }
constexpr Square    move_to(Move m)   { return Square((m >> 6) & 63u); }
constexpr PieceType move_promo(Move m) {
    const PieceType special = PieceType((m >> 12) & 15u);
    return special >= KNIGHT && special <= QUEEN ? special : NONE_PTYPE;
}
constexpr bool is_promo(Move m) { return move_promo(m) != NONE_PTYPE; }
constexpr bool is_ep_move(Move m) {
    return ((m >> 12) & 15u) == static_cast<Move>(PAWN);
}

static_assert(sizeof(Move) == 2);

} // namespace SHAYVERI

#endif // MOVE_H
