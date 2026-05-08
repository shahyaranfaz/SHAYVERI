#include "nnue_update.h"

#include <cstdlib>

namespace SHAYVERI {
namespace NNUE {

namespace {

int piece_type_index(PieceType pt) {
    return static_cast<int>(pt) - 1;
}

void acc_add(Accumulator &acc, Piece p, Square sq) {
    int wi = 0;
    int bi = 0;
    chess768_indices(piece_type_index(get_type(p)), static_cast<int>(get_colour(p)),
                     sq, wi, bi);
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        acc.vals[0][i] += feature_weights[wi][i];
        acc.vals[1][i] += feature_weights[bi][i];
    }
}

void acc_sub(Accumulator &acc, Piece p, Square sq) {
    int wi = 0;
    int bi = 0;
    chess768_indices(piece_type_index(get_type(p)), static_cast<int>(get_colour(p)),
                     sq, wi, bi);
    for (int i = 0; i < HIDDEN_SIZE; ++i) {
        acc.vals[0][i] -= feature_weights[wi][i];
        acc.vals[1][i] -= feature_weights[bi][i];
    }
}

Piece promoted_piece(Colour stm, PieceType promo) {
    if (stm == WHITE) {
        if (promo == KNIGHT) return WN;
        if (promo == BISHOP) return WB;
        if (promo == ROOK)   return WR;
        return WQ;
    }

    if (promo == KNIGHT) return BN;
    if (promo == BISHOP) return BB;
    if (promo == ROOK)   return BR;
    return BQ;
}

} // namespace

void update_accumulator(Accumulator &child, const Accumulator &parent,
                        const Board &b, Move m) {
    child = parent;

    Square from = move_from(m);
    Square to = move_to(m);
    Piece moved = b.get_piece(from);
    Piece captured = b.get_piece(to);
    PieceType promo = move_promo(m);
    Colour stm = b.side_to_move;

    if (is_ep_move(m)) {
        Square cap_sq = (stm == WHITE) ? to - 8 : to + 8;
        Piece ep_pawn = b.get_piece(cap_sq);
        acc_sub(child, ep_pawn, cap_sq);
        acc_sub(child, moved, from);
        acc_add(child, moved, to);
        return;
    }

    if (get_type(moved) == KING && std::abs(get_file(from) - get_file(to)) == 2) {
        Square rook_from = SQ_NONE;
        Square rook_to = SQ_NONE;
        Piece rook_piece = NONE_PIECE;

        if (to == make_square(FILE_G, RANK_1)) {
            rook_from = make_square(FILE_H, RANK_1);
            rook_to = make_square(FILE_F, RANK_1);
            rook_piece = WR;
        } else if (to == make_square(FILE_C, RANK_1)) {
            rook_from = make_square(FILE_A, RANK_1);
            rook_to = make_square(FILE_D, RANK_1);
            rook_piece = WR;
        } else if (to == make_square(FILE_G, RANK_8)) {
            rook_from = make_square(FILE_H, RANK_8);
            rook_to = make_square(FILE_F, RANK_8);
            rook_piece = BR;
        } else {
            rook_from = make_square(FILE_A, RANK_8);
            rook_to = make_square(FILE_D, RANK_8);
            rook_piece = BR;
        }

        acc_sub(child, moved, from);
        acc_add(child, moved, to);
        acc_sub(child, rook_piece, rook_from);
        acc_add(child, rook_piece, rook_to);
        return;
    }

    if (captured != NONE_PIECE)
        acc_sub(child, captured, to);

    acc_sub(child, moved, from);
    if (promo != NONE_PTYPE) acc_add(child, promoted_piece(stm, promo), to);
    else                     acc_add(child, moved, to);
}

} // namespace NNUE
} // namespace SHAYVERI
