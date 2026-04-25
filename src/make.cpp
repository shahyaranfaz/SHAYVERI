#include "make.h"

#include "attacks.h"

#include <cassert>

#include "zobrist.h"

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

static Piece promo_piece(Colour side_to_move, PieceType promo) {
    if (side_to_move == WHITE) {
        if (promo == KNIGHT) return WN;
        if (promo == BISHOP) return WB;
        if (promo == ROOK)   return WR;
        if (promo == QUEEN)  return WQ;
    } else {
        if (promo == KNIGHT) return BN;
        if (promo == BISHOP) return BB;
        if (promo == ROOK)   return BR;
        if (promo == QUEEN)  return BQ;
    }
    return NONE_PIECE;
}

static void remove_piece(Board &b, Piece p, Square sq) {
    b.bit_boards[p] &= ~bb_square(sq);
    b.occupancies[get_colour(p)] &= ~bb_square(sq); // Update occupancy
    b.occupied &= ~bb_square(sq); // Update total occupancy
    b.mailbox[sq] = NONE_PIECE; // Update mailbox
    b.hash ^= Zobrist::pieces[p][sq]; // Keep hash in sync
}

static void add_piece(Board &b, Piece p, Square sq) {
    b.bit_boards[p] |= bb_square(sq);
    b.occupancies[get_colour(p)] |= bb_square(sq);
    b.occupied |= bb_square(sq);
    b.mailbox[sq] = p;
    b.hash ^= Zobrist::pieces[p][sq];
}

void update_castling(Board &b, Square from, Square to) {
    b.hash ^= Zobrist::castlings[b.castling];
    b.castling &= CASTLING_RIGHTS_MASK[from];
    b.castling &= CASTLING_RIGHTS_MASK[to]; // Important: handles rook captures
    b.hash ^= Zobrist::castlings[b.castling];
}

bool make_move(Board &b, Move m, Undo &u) {
    Square from = move_from(m);
    Square to = move_to(m);
    PieceType promo = move_promo(m);
    Piece moved = b.get_piece(from);
    Piece captured = b.get_piece(to);

    // 1. Store state for undo
    u.hash = b.hash;
    u.castling = b.castling;
    u.en_passant = b.en_passant;
    u.half_move = b.half_move;
    u.full_move = b.full_move;
    u.captured = captured;
    u.was_ep = false;
    u.was_castle = false;

    // 2. Handle En Passant Hash
    if (b.en_passant != SQ_NONE) {
        b.hash ^= Zobrist::en_passants[get_file(b.en_passant)];
    }

    // 3. Handle captures
    if (captured != NONE_PIECE) {
        remove_piece(b, captured, to);
        b.half_move = 0;
    } else if (get_type(moved) == PAWN && to == b.en_passant) {
        u.was_ep = true;
        Square cap_sq = (b.side_to_move == WHITE) ? to - 8 : to + 8;
        u.captured = b.get_piece(cap_sq);
        remove_piece(b, u.captured, cap_sq);
        b.half_move = 0;
    }

    // 4. Handle Castling movement
    if (get_type(moved) == KING && std::abs(get_file(from) - get_file(to)) == 2) {
        u.was_castle = true;
        if (to == make_square(FILE_G, RANK_1)) {
            remove_piece(b, WR, make_square(FILE_H, RANK_1));
            add_piece(b, WR, make_square(FILE_F, RANK_1));
        }
        else if (to == make_square(FILE_C, RANK_1)) {
            remove_piece(b, WR, make_square(FILE_A, RANK_1));
            add_piece(b, WR, make_square(FILE_D, RANK_1));
        }
        else if (to == make_square(FILE_G, RANK_8)) {
            remove_piece(b, BR, make_square(FILE_H, RANK_8));
            add_piece(b, BR, make_square(FILE_F, RANK_8));
        }
        else if (to == make_square(FILE_C, RANK_8)) {
            remove_piece(b, BR, make_square(FILE_A, RANK_8));
            add_piece(b, BR, make_square(FILE_D, RANK_8));
        }
    }

    // 5. Update Castling rights hash/state
    update_castling(b, from, to);

    // 6. Move the piece
    remove_piece(b, moved, from);
    if (promo != NONE_PTYPE) {
        add_piece(b, promo_piece(b.side_to_move, promo), to);
    } else {
        add_piece(b, moved, to);
    }

    // 7. Update En Passant square
    b.en_passant = SQ_NONE;
    if (get_type(moved) == PAWN) {
        b.half_move = 0;
        if (std::abs(from - to) == 16) {
            b.en_passant = (from + to) / 2;
            b.hash ^= Zobrist::en_passants[get_file(b.en_passant)];
        }
    } else {
        b.half_move++;
    }

    // 8. Final state updates
    if (b.side_to_move == BLACK) b.full_move++;
    b.side_to_move = flip(b.side_to_move);
    b.hash ^= Zobrist::sides;

    // 9. Legality check
    if (is_square_attacked(b, king_square(b, flip(b.side_to_move)), b.side_to_move)) {
        unmake_move(b, m, u);
        return false;
    }

    return true;
}

void unmake_move(Board &b, Move m, const Undo &u) {
    b.side_to_move = flip(b.side_to_move);

    Square from = move_from(m);
    Square to = move_to(m);
    Piece moved_to_sq = b.get_piece(to);

    // 1. Reverse piece movement (handling promotion)
    remove_piece(b, moved_to_sq, to);
    Piece original_piece = (move_promo(m) != NONE_PTYPE)
                           ? (b.side_to_move == WHITE ? WP : BP)
                           : moved_to_sq;
    add_piece(b, original_piece, from);

    // 2. Restore captured piece
    if (u.was_ep) {
        Square cap_sq = (b.side_to_move == WHITE) ? to - 8 : to + 8;
        add_piece(b, u.captured, cap_sq);
    } else if (u.captured != NONE_PIECE) {
        add_piece(b, u.captured, to);
    }

    // 3. Reverse castling rook move
    if (u.was_castle) {
        if (to == make_square(FILE_G, RANK_1)) {
            remove_piece(b, WR, make_square(FILE_F, RANK_1));
            add_piece(b, WR, make_square(FILE_H, RANK_1));
        }
        else if (to == make_square(FILE_C, RANK_1)) {
            remove_piece(b, WR, make_square(FILE_D, RANK_1));
            add_piece(b, WR, make_square(FILE_A, RANK_1));
        }
        else if (to == make_square(FILE_G, RANK_8)) {
            remove_piece(b, BR, make_square(FILE_F, RANK_8));
            add_piece(b, BR, make_square(FILE_H, RANK_8));
        }
        else if (to == make_square(FILE_C, RANK_8)) {
            remove_piece(b, BR, make_square(FILE_D, RANK_8));
            add_piece(b, BR, make_square(FILE_A, RANK_8));
        }
    }

    // 4. Restore state variables
    b.castling = u.castling;
    b.en_passant = u.en_passant;
    b.half_move = u.half_move;
    b.full_move = u.full_move;
    b.hash = u.hash;
}
