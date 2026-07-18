#include "make.h"

#include "attacks.h"
#include "zobrist.h"

#include <cassert>

namespace SHAYVERI {

static const int CASTLING_RIGHTS_MASK[64] = {
    13, 15, 15, 15, 12, 15, 15, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
     7, 15, 15, 15,  3, 15, 15, 11
};

static void remove_piece(Board &b, Piece p, Square sq) {
    b.bit_boards[p]               &= ~bb_square(sq);
    b.occupancies[get_colour(p)]  &= ~bb_square(sq);
    b.occupied                    &= ~bb_square(sq);
    b.mailbox[sq]                  = NONE_PIECE;
    b.hash                        ^= Zobrist::pieces[p][sq];
}

static void add_piece(Board &b, Piece p, Square sq) {
    b.bit_boards[p]              |= bb_square(sq);
    b.occupancies[get_colour(p)] |= bb_square(sq);
    b.occupied                   |= bb_square(sq);
    b.mailbox[sq]                 = p;
    b.hash                       ^= Zobrist::pieces[p][sq];
}

static void update_castling(Board &b, Square from, Square to) {
    b.hash     ^= Zobrist::castlings[b.castling];
    b.castling &= CASTLING_RIGHTS_MASK[from];
    b.castling &= CASTLING_RIGHTS_MASK[to];
    b.hash     ^= Zobrist::castlings[b.castling];
}

bool make_move(Board &b, Move m, Undo &u) {
    Square    from     = move_from(m);
    Square    to       = move_to(m);
    PieceType promo    = move_promo(m);

    if (!is_valid(from) || !is_valid(to))
        return false;

    Piece     moved    = b.get_piece(from);
    Piece     captured = b.get_piece(to);
    const bool is_castle = get_type(moved) == KING &&
        std::abs(get_file(from) - get_file(to)) == 2;
    CastleInfo castle{};

    if (moved == NONE_PIECE || get_colour(moved) != b.side_to_move)
        return false;
    if (captured != NONE_PIECE && get_colour(captured) == b.side_to_move)
        return false;
    if (promo != NONE_PTYPE) {
        if (promo != KNIGHT && promo != BISHOP && promo != ROOK && promo != QUEEN)
            return false;
        if (get_type(moved) != PAWN)
            return false;
        Rank promo_rank = get_rank(to);
        if ((b.side_to_move == WHITE && promo_rank != RANK_8) ||
            (b.side_to_move == BLACK && promo_rank != RANK_1))
            return false;
    }
    if (is_ep_move(m)) {
        if (get_type(moved) != PAWN || to != b.en_passant || captured != NONE_PIECE)
            return false;
        Square cap_sq = (b.side_to_move == WHITE) ? to - 8 : to + 8;
        if (!is_valid(cap_sq) || b.get_piece(cap_sq) != (b.side_to_move == WHITE ? BP : WP))
            return false;
    }
    if (is_castle) {
        const Rank home = b.side_to_move == WHITE ? RANK_1 : RANK_8;
        const bool kingside = to == make_square(FILE_G, home);
        if (from != make_square(FILE_E, home) ||
            (!kingside && to != make_square(FILE_C, home))) return false;
        castle = castle_info(b.side_to_move, kingside);
        if (!(b.castling & castle.required_right) ||
            b.get_piece(castle.rook_from) != castle.rook) return false;
    }

    u.hash       = b.hash;
    u.castling   = b.castling;
    u.en_passant = b.en_passant;
    u.half_move  = b.half_move;
    u.full_move  = b.full_move;
    u.captured   = captured;
    u.was_ep     = false;
    u.was_castle = false;

    if (b.en_passant != SQ_NONE)
        b.hash ^= Zobrist::en_passants[get_file(b.en_passant)];

    if (captured != NONE_PIECE) {
        remove_piece(b, captured, to);
        b.half_move = 0;
    } else if (is_ep_move(m)) {
        u.was_ep = true;
        Square cap_sq = (b.side_to_move == WHITE) ? to - 8 : to + 8;
        u.captured = b.get_piece(cap_sq);
        remove_piece(b, u.captured, cap_sq);
        b.half_move = 0;
    }

    if (is_castle) {
        u.was_castle = true;
        remove_piece(b, castle.rook, castle.rook_from);
        add_piece(b, castle.rook, castle.rook_to);
    }

    update_castling(b, from, to);
    remove_piece(b, moved, from);
    if (promo != NONE_PTYPE) add_piece(b, promotion_piece(b.side_to_move, promo), to);
    else                     add_piece(b, moved, to);

    b.en_passant = SQ_NONE;
    if (get_type(moved) == PAWN) {
        b.half_move = 0;
        if (std::abs(from - to) == 16) {
            b.en_passant = (from + to) / 2;
            b.hash ^= Zobrist::en_passants[get_file(b.en_passant)];
        }
    } else {
        if (captured == NONE_PIECE) b.half_move++;
    }

    if (b.side_to_move == BLACK) b.full_move++;
    b.side_to_move = flip(b.side_to_move);
    b.hash ^= Zobrist::sides;

    if (is_square_attacked(b, king_square(b, flip(b.side_to_move)), b.side_to_move)) {
        unmake_move(b, m, u);
        return false;
    }
    return true;
}

void unmake_move(Board &b, Move m, const Undo &u) {
    b.side_to_move = flip(b.side_to_move);

    Square from        = move_from(m);
    Square to          = move_to(m);
    Piece  moved_to_sq = b.get_piece(to);

    remove_piece(b, moved_to_sq, to);
    Piece original = (move_promo(m) != NONE_PTYPE)
                     ? (b.side_to_move == WHITE ? WP : BP)
                     : moved_to_sq;
    add_piece(b, original, from);

    if (u.was_ep) {
        Square cap_sq = (b.side_to_move == WHITE) ? to - 8 : to + 8;
        add_piece(b, u.captured, cap_sq);
    } else if (u.captured != NONE_PIECE) {
        add_piece(b, u.captured, to);
    }

    if (u.was_castle) {
        const CastleInfo castle = castle_info(b.side_to_move, get_file(to) == FILE_G);
        remove_piece(b, castle.rook, castle.rook_to);
        add_piece(b, castle.rook, castle.rook_from);
    }

    b.castling   = u.castling;
    b.en_passant = u.en_passant;
    b.half_move  = u.half_move;
    b.full_move  = u.full_move;
    b.hash       = u.hash;
}

} // namespace SHAYVERI
