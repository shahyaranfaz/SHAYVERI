#include "see.h"

#include "attacks.h"
#include "make.h"
#include "move.h"

#include <algorithm>

// values by PieceType index (NONE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING)
static constexpr int SEE_VALUES[7] = {0, 100, 320, 330, 500, 900, 20000};

static inline U64 attackers_to(const Board& b, Square sq, U64 occ) {
    int f = get_file(sq), r = get_rank(sq);

    U64 pawns = 0;
    if (r > 0) {
        if (f > 0) pawns |= bb_square(make_square(File(f - 1), Rank(r - 1)));
        if (f < 7) pawns |= bb_square(make_square(File(f + 1), Rank(r - 1)));
    }
    U64 wp_atk = pawns & b.bit_boards[WP];

    pawns = 0;
    if (r < 7) {
        if (f > 0) pawns |= bb_square(make_square(File(f - 1), Rank(r + 1)));
        if (f < 7) pawns |= bb_square(make_square(File(f + 1), Rank(r + 1)));
    }

    U64 bp_atk = pawns & b.bit_boards[BP];
    U64 n_atk = knight_attacks(sq);
    U64 k_atk = king_attacks(sq);
    U64 b_atk = bishop_attacks(sq, occ);
    U64 r_atk = rook_attacks(sq, occ);

    U64 attacks = 0;
    attacks |= wp_atk | bp_atk;
    attacks |= n_atk & (b.bit_boards[WN] | b.bit_boards[BN]);
    attacks |= k_atk & (b.bit_boards[WK] | b.bit_boards[BK]);
    attacks |= b_atk & (b.bit_boards[WB] | b.bit_boards[BB] | b.bit_boards[WQ] | b.bit_boards[BQ]);
    attacks |= r_atk & (b.bit_boards[WR] | b.bit_boards[BR] | b.bit_boards[WQ] | b.bit_boards[BQ]);

    return attacks & occ;
}

static inline Piece piece_on_square(const Board& b, Square sq) {
    return b.get_piece(sq);
}

static inline int ptype_value(Piece p) {
    return SEE_VALUES[get_type(p)];
}

static inline U64 pick_least_valuable_attacker(const Board &b, Colour side, U64 atks, Piece &piece_out) {
    auto pick_from_bb = [&](U64 bb, Piece p) -> U64 {
        if (!bb) return 0;
        Square s = __builtin_ctzll(bb);
        piece_out = p;
        return bb_square(s);
    };

    if (side == WHITE) {
        if (U64 bb = atks & b.bit_boards[WP]) return pick_from_bb(bb, WP);
        if (U64 bb = atks & b.bit_boards[WN]) return pick_from_bb(bb, WN);
        if (U64 bb = atks & b.bit_boards[WB]) return pick_from_bb(bb, WB);
        if (U64 bb = atks & b.bit_boards[WR]) return pick_from_bb(bb, WR);
        if (U64 bb = atks & b.bit_boards[WQ]) return pick_from_bb(bb, WQ);
        if (U64 bb = atks & b.bit_boards[WK]) return pick_from_bb(bb, WK);
    } else {
        if (U64 bb = atks & b.bit_boards[BP]) return pick_from_bb(bb, BP);
        if (U64 bb = atks & b.bit_boards[BN]) return pick_from_bb(bb, BN);
        if (U64 bb = atks & b.bit_boards[BB]) return pick_from_bb(bb, BB);
        if (U64 bb = atks & b.bit_boards[BR]) return pick_from_bb(bb, BR);
        if (U64 bb = atks & b.bit_boards[BQ]) return pick_from_bb(bb, BQ);
        if (U64 bb = atks & b.bit_boards[BK]) return pick_from_bb(bb, BK);
    }

    piece_out = NONE_PIECE;
    return 0;
}

int see(const Board& b, Move m) {
    Square from = move_from(m);
    Square to = move_to(m);

    Piece attacker_orig = piece_on_square(b, from);
    Piece captured_orig = piece_on_square(b, to);

    if (attacker_orig == NONE_PIECE || captured_orig == NONE_PIECE) return 0;

    int gain[32];
    int depth = 0;
    U64 occ = b.occupied;
    occ &= ~bb_square(from);
    gain[depth] = ptype_value(captured_orig);

    Colour side = flip(get_colour(attacker_orig));
    occ |= bb_square(to);
    U64 atks = attackers_to(b, to, occ) & ~bb_square(to);
    Piece last_attacker = attacker_orig;
    depth++;

    while (true) {
        Piece picked_piece;
        U64 pick = pick_least_valuable_attacker(b, side, atks, picked_piece);
        if (!pick) break;

        gain[depth] = ptype_value(last_attacker) - gain[depth - 1];
        if (std::max(-gain[depth - 1], gain[depth]) < 0) break;

        occ &= ~pick;

        atks &= ~pick;
        atks = attackers_to(b, to, occ) & ~bb_square(to);

        last_attacker = picked_piece;
        side = flip(side);
        depth++;
        if (depth >= 31) break;
    }

    while (--depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);
    return gain[0];
}
