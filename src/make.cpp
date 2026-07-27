#include "make.h"

#include "attacks.h"
#include "move_gen.h"
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

static void remove_piece_no_hash(Board &b, Piece p, Square sq) {
    b.bit_boards[p]               &= ~bb_square(sq);
    b.occupancies[get_colour(p)]  &= ~bb_square(sq);
    b.occupied                    &= ~bb_square(sq);
    b.mailbox[sq]                  = NONE_PIECE;
}

static void add_piece_no_hash(Board &b, Piece p, Square sq) {
    b.bit_boards[p]              |= bb_square(sq);
    b.occupancies[get_colour(p)] |= bb_square(sq);
    b.occupied                   |= bb_square(sq);
    b.mailbox[sq]                 = p;
}

static void remove_piece(Board &b, Piece p, Square sq) {
    remove_piece_no_hash(b, p, sq);
    b.hash                        ^= Zobrist::pieces[p][sq];
    if (get_type(p) == PAWN)
        b.pawn_hash ^= Zobrist::pieces[p][sq];
}

static void add_piece(Board &b, Piece p, Square sq) {
    add_piece_no_hash(b, p, sq);
    b.hash                       ^= Zobrist::pieces[p][sq];
    if (get_type(p) == PAWN)
        b.pawn_hash ^= Zobrist::pieces[p][sq];
}

static void update_castling(Board &b, Square from, Square to) {
    b.hash     ^= Zobrist::castlings[b.castling];
    b.castling &= CASTLING_RIGHTS_MASK[from];
    b.castling &= CASTLING_RIGHTS_MASK[to];
    b.hash     ^= Zobrist::castlings[b.castling];
}

static bool make_generated_move_impl(
    Board &b, Move m, Undo &u, bool verify_king_safety) {
    Square from = move_from(m);
    Square to = move_to(m);
    PieceType promo    = move_promo(m);

    assert(is_valid(from) && is_valid(to));

    Piece moved = b.get_piece(from);
    Piece captured = b.get_piece(to);
    const bool is_castle = get_type(moved) == KING &&
        std::abs(get_file(from) - get_file(to)) == 2;
    CastleInfo castle{};

    assert(moved != NONE_PIECE && get_colour(moved) == b.side_to_move);
    assert(captured == NONE_PIECE || get_colour(captured) != b.side_to_move);
    if (promo != NONE_PTYPE) {
        assert(promo == KNIGHT || promo == BISHOP || promo == ROOK || promo == QUEEN);
        assert(get_type(moved) == PAWN);
        assert((b.side_to_move == WHITE && get_rank(to) == RANK_8)
            || (b.side_to_move == BLACK && get_rank(to) == RANK_1));
    }
    if (is_ep_move(m)) {
        assert(get_type(moved) == PAWN && to == b.en_passant
            && captured == NONE_PIECE);
        assert(is_valid((b.side_to_move == WHITE) ? to - 8 : to + 8)
            && b.get_piece((b.side_to_move == WHITE) ? to - 8 : to + 8)
                == (b.side_to_move == WHITE ? BP : WP));
    }
    if (is_castle) {
        const Rank home = b.side_to_move == WHITE ? RANK_1 : RANK_8;
        const bool kingside = to == make_square(FILE_G, home);
        assert(from == make_square(FILE_E, home)
            && (kingside || to == make_square(FILE_C, home)));
        castle = castle_info(b.side_to_move, kingside);
        assert((b.castling & castle.required_right)
            && b.get_piece(castle.rook_from) == castle.rook);
    }

    u.hash       = b.hash;
    u.castling   = b.castling;
    u.en_passant = b.en_passant;
    u.half_move  = b.half_move;
    u.full_move  = b.full_move;
    u.captured   = captured;
    u.was_ep     = false;
    u.was_castle = false;
    u.pawn_hash  = b.pawn_hash;

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
    else add_piece(b, moved, to);

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

    if (verify_king_safety
        && is_square_attacked(
            b, king_square(b, flip(b.side_to_move)), b.side_to_move)) {
        unmake_move(b, m, u);
        return false;
    }
    assert(b.is_consistent());
    return true;
}

bool make_generated_move(Board &b, Move m, Undo &u) {
    return make_generated_move_impl(b, m, u, true);
}

void make_legal_move(Board &b, Move m, Undo &u) {
#ifdef SHAYVERI_VERIFY_LEGAL_MAKE
    const bool made = make_generated_move_impl(b, m, u, true);
#else
    const bool made = make_generated_move_impl(b, m, u, false);
#endif
    assert(made);
    (void)made;
}

bool make_move(Board &b, Move m, Undo &u) {
    if (m == MOVE_NONE) return false;
    const MoveList pseudo = generate_pseudo_legal_moves(b);
    for (int i = 0; i < pseudo.count; ++i) {
        if (pseudo.moves[i] == m)
            return make_generated_move(b, m, u);
    }
    return false;
}

void unmake_move(Board &b, Move m, const Undo &u) {
    b.side_to_move = flip(b.side_to_move);

    Square from        = move_from(m);
    Square to          = move_to(m);
    Piece moved_to_sq = b.get_piece(to);

    remove_piece_no_hash(b, moved_to_sq, to);
    Piece original = (move_promo(m) != NONE_PTYPE)
                     ? (b.side_to_move == WHITE ? WP : BP)
                     : moved_to_sq;
    add_piece_no_hash(b, original, from);

    if (u.was_ep) {
        Square cap_sq = (b.side_to_move == WHITE) ? to - 8 : to + 8;
        add_piece_no_hash(b, u.captured, cap_sq);
    } else if (u.captured != NONE_PIECE) {
        add_piece_no_hash(b, u.captured, to);
    }

    if (u.was_castle) {
        const CastleInfo castle = castle_info(b.side_to_move, get_file(to) == FILE_G);
        remove_piece_no_hash(b, castle.rook, castle.rook_to);
        add_piece_no_hash(b, castle.rook, castle.rook_from);
    }

    b.castling   = u.castling;
    b.en_passant = u.en_passant;
    b.half_move  = u.half_move;
    b.full_move  = u.full_move;
    b.hash       = u.hash;
    b.pawn_hash  = u.pawn_hash;
    assert(b.is_consistent());
}

void make_null_move(Board &b, Undo &u) {
    u.hash = b.hash;
    u.pawn_hash = b.pawn_hash;
    u.en_passant = b.en_passant;
    u.half_move = b.half_move;
    u.castling = b.castling;
    u.captured = NONE_PIECE;
    u.was_ep = false;
    u.was_castle = false;

    if (b.en_passant != SQ_NONE) {
        b.hash ^= Zobrist::en_passants[get_file(b.en_passant)];
        b.en_passant = SQ_NONE;
    }
    b.side_to_move = flip(b.side_to_move);
    b.hash ^= Zobrist::sides;
    ++b.half_move;
    assert(b.is_consistent());
}

void unmake_null_move(Board &b, const Undo &u) {
    b.side_to_move = flip(b.side_to_move);
    b.en_passant = u.en_passant;
    b.half_move = u.half_move;
    b.castling = u.castling;
    b.hash = u.hash;
    b.pawn_hash = u.pawn_hash;
    assert(b.is_consistent());
}

} // namespace SHAYVERI
