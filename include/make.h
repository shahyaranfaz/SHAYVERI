#ifndef MAKE_H
#define MAKE_H

#include "board.h"
#include "move.h"

namespace SHAYVERI {

struct Undo {
    int    castling;
    Square en_passant;
    int    half_move;
    int    full_move;
    Piece  captured;
    bool   was_ep;
    bool   was_castle;
    U64    hash;
};

struct CastleInfo {
    Piece rook;
    Square rook_from;
    Square rook_to;
    int required_right;
};

constexpr CastleInfo castle_info(Colour side, bool kingside) {
    const Rank rank = side == WHITE ? RANK_1 : RANK_8;
    return {
        side == WHITE ? WR : BR,
        make_square(kingside ? FILE_H : FILE_A, rank),
        make_square(kingside ? FILE_F : FILE_D, rank),
        side == WHITE
            ? (kingside ? WHITE_KINGSIDE : WHITE_QUEENSIDE)
            : (kingside ? BLACK_KINGSIDE : BLACK_QUEENSIDE),
    };
}

constexpr Piece promotion_piece(Colour side, PieceType promotion) {
    if (promotion < KNIGHT || promotion > QUEEN) return NONE_PIECE;
    return static_cast<Piece>(static_cast<int>(promotion) + (side == BLACK ? 6 : 0));
}

bool make_move(Board &b, Move m, Undo &u);
void unmake_move(Board &b, Move m, const Undo &u);

} // namespace SHAYVERI

#endif // MAKE_H
