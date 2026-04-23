#include "make.h"

#include "attacks.h"

#include <cassert>

#include "zobrist.h"

static Piece promo_piece(Colour side_to_move, PieceType promo) {
    // promo: 1=N,2=B,3=R,4=Q
    if (side_to_move == WHITE) {
        if (promo == 1) return WN;
        if (promo == 2) return WB;
        if (promo == 3) return WR;
        if (promo == 4) return WQ;
    } else {
        if (promo == 1) return BN;
        if (promo == 2) return BB;
        if (promo == 3) return BR;
        if (promo == 4) return BQ;
    }
    return NONE_PIECE;
}

static void remove_piece(Board &b, Piece p, Square sq) {
    b.bit_boards[p] &= ~bb_square(sq);
}
static void add_piece(Board &b, Piece p, Square sq) {
    b.bit_boards[p] |= bb_square(sq);
}

static void clear_castling_if_rook_king_moved(Board &b, Piece moved, Square from) {
    if (moved == WK)
        b.castling &= ~(WHITE_KINGSIDE | WHITE_QUEENSIDE);
    else if (moved == BK)
        b.castling &= ~(BLACK_KINGSIDE | BLACK_QUEENSIDE);
    else if (moved == WR) {
        if (from == make_square(FILE_H, RANK_1)) b.castling &= ~WHITE_KINGSIDE;
        if (from == make_square(FILE_A, RANK_1)) b.castling &= ~WHITE_QUEENSIDE;
    } else if (moved == BR) {
        if (from == make_square(FILE_H, RANK_8)) b.castling &= ~BLACK_KINGSIDE;
        if (from == make_square(FILE_A, RANK_8)) b.castling &= ~BLACK_QUEENSIDE;
    }
}

static void clear_castling_if_rook_captured(Board &b, Piece captured, Square to) {
    if (captured == WR) {
        if (to == make_square(FILE_H, RANK_1))
            b.castling &= ~WHITE_KINGSIDE;
        if (to == make_square(FILE_A, RANK_1))
            b.castling &= ~WHITE_QUEENSIDE;
    } else if (captured == BR) {
        if (to == make_square(FILE_H, RANK_8))
            b.castling &= ~BLACK_KINGSIDE;
        if (to == make_square(FILE_A, RANK_8))
            b.castling &= ~BLACK_QUEENSIDE;
    }
}

bool make_move(Board &b, Move m, Undo &u) {
    u.castling = b.castling;
    u.en_passant = b.en_passant;
    u.half_move = b.half_move;
    u.full_move = b.full_move;
    u.hash = b.hash;
    u.captured = NONE_PIECE;

    Square from = move_from(m);
    Square to = move_to(m);
    PieceType promo = move_promo(m);

    Piece moved = b.get_piece(from);
    if (moved == NONE_PIECE) return false;
    if (b.side_to_move != get_colour(moved)) return false;

    const int old_castling = b.castling & 15;
    const Square old_ep = b.en_passant;
    b.hash ^= Zobrist::castlings[old_castling];
    if (old_ep != SQ_NONE) b.hash ^= Zobrist::en_passants[get_file(old_ep)];

    Piece captured = b.get_piece(to);

    bool is_ep = false;
    if ((moved == WP || moved == BP) && to == b.en_passant && captured == NONE_PIECE) {
        is_ep = true;
        Square cap = (b.side_to_move == WHITE) ? (to - 8) : (to + 8);
        captured = b.get_piece(cap);
        if (captured == NONE_PIECE) { b.hash = u.hash; return false; }
        u.captured = captured;
        b.hash ^= Zobrist::pieces[captured][cap];
        remove_piece(b, captured, cap);
    } else if (captured != NONE_PIECE) {
        u.captured = captured;
        b.hash ^= Zobrist::pieces[captured][to];
        remove_piece(b, captured, to);
        clear_castling_if_rook_captured(b, captured, to);
    }
    u.was_ep = is_ep;
    b.hash ^= Zobrist::pieces[moved][from];
    remove_piece(b, moved, from);

    if (promo && (moved == WP || moved == BP)) {
        Piece pp = promo_piece(b.side_to_move, promo);
        if (pp == NONE_PIECE) { b.hash = u.hash; return false; }
        b.hash ^= Zobrist::pieces[pp][to];
        add_piece(b, pp, to);
    } else {
        b.hash ^= Zobrist::pieces[moved][to];
        add_piece(b, moved, to);
    }

    bool is_castle = (moved == WK || moved == BK) && (from == make_square(FILE_E, (b.side_to_move==WHITE?RANK_1:RANK_8))) &&
                     (to == make_square(FILE_G, (b.side_to_move==WHITE?RANK_1:RANK_8)) ||
                      to == make_square(FILE_C, (b.side_to_move==WHITE?RANK_1:RANK_8)));
    if (is_castle) {
        if (b.side_to_move == WHITE) {
            if (to == make_square(FILE_G, RANK_1)) { // king side
                Square rook_from = make_square(FILE_H, RANK_1);
                Square rook_to = make_square(FILE_F, RANK_1);
                b.hash ^= Zobrist::pieces[WR][rook_from];
                b.hash ^= Zobrist::pieces[WR][rook_to];
                remove_piece(b, WR, rook_from);
                add_piece(b, WR, rook_to);
            } else { // queen side
                Square rook_from = make_square(FILE_A, RANK_1);
                Square rook_to = make_square(FILE_D, RANK_1);
                b.hash ^= Zobrist::pieces[WR][rook_from];
                b.hash ^= Zobrist::pieces[WR][rook_to];
                remove_piece(b, WR, rook_from);
                add_piece(b, WR, rook_to);
            }
            b.castling &= ~(WHITE_KINGSIDE | WHITE_QUEENSIDE);
        } else {
            if (to == make_square(FILE_G, RANK_8)) {
                Square rook_from = make_square(FILE_H, RANK_8);
                Square rook_to = make_square(FILE_F, RANK_8);
                b.hash ^= Zobrist::pieces[BR][rook_from];
                b.hash ^= Zobrist::pieces[BR][rook_to];
                remove_piece(b, BR, rook_from);
                add_piece(b, BR, rook_to);
            } else {
                Square rook_from = make_square(FILE_A, RANK_8);
                Square rook_to = make_square(FILE_D, RANK_8);
                b.hash ^= Zobrist::pieces[BR][rook_from];
                b.hash ^= Zobrist::pieces[BR][rook_to];
                remove_piece(b, BR, rook_from);
                add_piece(b, BR, rook_to);
            }
            b.castling &= ~(BLACK_KINGSIDE | BLACK_QUEENSIDE);
        }
    }
    u.was_castle = is_castle;
    clear_castling_if_rook_king_moved(b, moved, from);

    b.en_passant = SQ_NONE;
    if (moved == WP && get_rank(from) == RANK_2 && get_rank(to) == RANK_4) b.en_passant = from + 8;
    if (moved == BP && get_rank(from) == RANK_7 && get_rank(to) == RANK_5) b.en_passant = from - 8;

    b.hash ^= Zobrist::castlings[b.castling & 15];
    if (b.en_passant != SQ_NONE) b.hash ^= Zobrist::en_passants[get_file(b.en_passant)];

    if (moved == WP || moved == BP || u.captured != NONE_PIECE) b.half_move = 0;
    else b.half_move++;

    if (b.side_to_move == BLACK) b.full_move++;

    b.side_to_move = flip(b.side_to_move);
    b.hash ^= Zobrist::sides;

    b.recompute_all();

#ifndef NDEBUG
    assert(b.hash == Zobrist::compute(b));
#endif

    Square ksq = king_square(b, flip(b.side_to_move));
    if (is_square_attacked(b, ksq, b.side_to_move)) {
        unmake_move(b, m, u);
        return false;
    }
    return true;
}

void unmake_move(Board &b, Move m, const Undo &u) {
    b.side_to_move = flip(b.side_to_move);
    Square from = move_from(m);
    Square to = move_to(m);
    PieceType promo = move_promo(m);

    b.castling = u.castling;
    b.en_passant = u.en_passant;
    b.half_move = u.half_move;
    b.full_move = u.full_move;
    b.hash = u.hash;

    Piece piece_to = b.get_piece(to);

    // undo rook move if castling
    if (u.was_castle) {
        if (b.side_to_move == WHITE) {
            if (to == make_square(FILE_G, RANK_1)) {
                remove_piece(b, WR, make_square(FILE_F, RANK_1));
                add_piece(b, WR, make_square(FILE_H, RANK_1));
            } else {
                remove_piece(b, WR, make_square(FILE_D, RANK_1));
                add_piece(b, WR, make_square(FILE_A, RANK_1));
            }
        } else {
            if (to == make_square(FILE_G, RANK_8)) {
                remove_piece(b, BR, make_square(FILE_F, RANK_8));
                add_piece(b, BR, make_square(FILE_H, RANK_8));
            } else {
                remove_piece(b, BR, make_square(FILE_D, RANK_8));
                add_piece(b, BR, make_square(FILE_A, RANK_8));
            }
        }
    }

    // undo piece move (handle promotion)
    remove_piece(b, piece_to, to);

    Piece moved_back = piece_to;
    if (promo) moved_back = (b.side_to_move == WHITE) ? WP : BP;
    add_piece(b, moved_back, from);

    if (u.captured != NONE_PIECE) {
        if (u.was_ep) {
            Square cap_square = (b.side_to_move == WHITE) ? (to - 8) : (to + 8);
            add_piece(b, u.captured, cap_square);
        } else {
            add_piece(b, u.captured, to);
        }
    }
    b.recompute_all();

#ifndef NDEBUG
    assert(b.hash == Zobrist::compute(b));
#endif

}
