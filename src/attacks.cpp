#include "attacks.h"
#include <cassert>

static bool is_valid_square(int f, int r) { return f >= 0 && f < 8 && r >= 0 && r < 8; }

U64 pawn_attacks(Colour c, Square from) {
    int f = get_file(from), r = get_rank(from);
    U64 bb = 0;
    if (c == WHITE) {
        if (is_valid_square(f - 1, r + 1))
            bb |= bb_square(make_square(File(f - 1), Rank(r + 1)));
        if (is_valid_square(f + 1, r + 1))
            bb |= bb_square(make_square(File(f + 1), Rank(r + 1)));
    } else {
        if (is_valid_square(f - 1, r - 1))
            bb |= bb_square(make_square(File(f - 1), Rank(r - 1)));
        if (is_valid_square(f + 1, r - 1))
            bb |= bb_square(make_square(File(f + 1), Rank(r - 1)));
    }
    return bb;
}

U64 knight_attacks(Square from) {
    static const int df[8] = { 1, 2, 2, 1, -1, -2, -2, -1 };
    static const int dr[8] = { 2, 1, -1, -2, -2, -1, 1, 2 };
    int f = get_file(from), r = get_rank(from);
    U64 bb = 0;
    for (int i = 0; i < 8; ++i) {
        int nf = f + df[i], nr = r + dr[i];
        if (is_valid_square(nf, nr))
            bb |= bb_square(make_square(File(nf), Rank(nr)));
    }
    return bb;
}

U64 king_attacks(Square from) {
    U64 bb = 0;
    int f = get_file(from), r = get_rank(from);
    for (int df = -1; df <= 1; ++df) {
        for (int dr = -1; dr <= 1; ++dr) {
            if (df == 0 && dr == 0) continue;
            int nf = f + df, nr = r + dr;
            if (is_valid_square(nf, nr))
                bb |= bb_square(make_square(File(nf), Rank(nr)));
        }
    }
    return bb;
}

static U64 ray_attacks(Square from, int df, int dr, U64 occupied) {
    U64 bb = 0;
    int f = get_file(from), r = get_rank(from);
    while (true) {
        f += df; r += dr;
        if (!is_valid_square(f, r)) break;
        Square sq = make_square(File(f), Rank(r));
        bb |= bb_square(sq);
        if (occupied & bb_square(sq)) break;
    }
    return bb;
}

U64 bishop_attacks(Square from, U64 occupied) {
    return ray_attacks(from, 1, 1, occupied) |
        ray_attacks(from, 1, -1, occupied) |
        ray_attacks(from, -1, 1, occupied) |
        ray_attacks(from, -1, -1, occupied);
}

U64 rook_attacks(Square from, U64 occupied) {
    return ray_attacks(from, 1, 0, occupied) |
        ray_attacks(from, -1, 0, occupied) |
        ray_attacks(from, 0, 1, occupied) |
        ray_attacks(from, 0, -1, occupied);
}

U64 queen_attacks(Square from, U64 occupied) {
    return bishop_attacks(from, occupied) | rook_attacks(from, occupied);
}

Square king_square(const Board& b, Colour c) {
    U64 kbb = (c == WHITE) ? b.bit_boards[WK] : b.bit_boards[BK];
    assert(kbb && "king missing");
    return __builtin_ctzll(kbb);
}

bool is_square_attacked(const Board& b, Square sq, Colour attacker) {
    if (attacker == WHITE) {
        int f = get_file(sq), r = get_rank(sq);
        if (r > 0) {
            if (f > 0 && (b.bit_boards[WP] & bb_square(make_square(File(f - 1), Rank(r - 1)))))
                return true;
            if (f < 7 && (b.bit_boards[WP] & bb_square(make_square(File(f + 1), Rank(r - 1)))))
                return true;
        }
    } else {
        int f = get_file(sq), r = get_rank(sq);
        if (r < 7) {
            if (f > 0 && (b.bit_boards[BP] & bb_square(make_square(File(f - 1), Rank(r + 1)))))
                return true;
            if (f < 7 && (b.bit_boards[BP] & bb_square(make_square(File(f + 1), Rank(r + 1)))))
                return true;
        }
    }

    U64 kn_att = knight_attacks(sq);
    if (attacker == WHITE) {
        if (kn_att & b.bit_boards[WN]) return true;
    } else {
        if (kn_att & b.bit_boards[BN]) return true;
    }

    U64 k_att = king_attacks(sq);
    if (attacker == WHITE) {
        if (k_att & b.bit_boards[WK]) return true;
    } else {
        if (k_att & b.bit_boards[BK]) return true;
    }

    U64 b_att = bishop_attacks(sq, b.occupied);
    if (attacker == WHITE) {
        if (b_att & (b.bit_boards[WB] | b.bit_boards[WQ])) return true;
    } else {
        if (b_att & (b.bit_boards[BB] | b.bit_boards[BQ])) return true;
    }

    U64 r_att = rook_attacks(sq, b.occupied);
    if (attacker == WHITE) {
        if (r_att & (b.bit_boards[WR] | b.bit_boards[WQ])) return true;
    } else {
        if (r_att & (b.bit_boards[BR] | b.bit_boards[BQ])) return true;
    }

    return false;
}
