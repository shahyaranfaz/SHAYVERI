#include "attacks.h"
#include <cassert>
#include <immintrin.h>

namespace SHAYVERI {

static bool is_valid_square(int f, int r) {
    return f >= 0 && f < 8 && r >= 0 && r < 8;
}

U64 pawn_attacks(Colour c, Square from) {
    int f = get_file(from), r = get_rank(from);
    U64 bb = 0;
    if (c == WHITE) {
        if (is_valid_square(f - 1, r + 1)) bb |= bb_square(make_square(File(f-1), Rank(r+1)));
        if (is_valid_square(f + 1, r + 1)) bb |= bb_square(make_square(File(f+1), Rank(r+1)));
    } else {
        if (is_valid_square(f - 1, r - 1)) bb |= bb_square(make_square(File(f-1), Rank(r-1)));
        if (is_valid_square(f + 1, r - 1)) bb |= bb_square(make_square(File(f+1), Rank(r-1)));
    }
    return bb;
}

U64 knight_attacks(Square from) {
    static const int df[8] = { 1,  2,  2,  1, -1, -2, -2, -1 };
    static const int dr[8] = { 2,  1, -1, -2, -2, -1,  1,  2 };
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
    int f = get_file(from), r = get_rank(from);
    U64 bb = 0;
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

// Ray system (precomputed sliding geometry)

U64 Ray[64][8];

enum Dir { N, S, E, W, NE, NW, SE, SW };

static void init_rays() {
    const int df[8] = { 0,  0, 1, -1,  1, -1,  1, -1 };
    const int dr[8] = { 1, -1, 0,  0,  1,  1, -1, -1 };
    for (int sq = 0; sq < 64; ++sq) {
        int f = sq % 8, r = sq / 8;
        for (int d = 0; d < 8; ++d) {
            int nf = f + df[d], nr = r + dr[d];
            while (is_valid_square(nf, nr)) {
                Ray[sq][d] |= bb_square(make_square(File(nf), Rank(nr)));
                nf += df[d]; nr += dr[d];
            }
        }
    }
}

static U64 bishop_mask(Square sq) {
    return Ray[sq][NE] | Ray[sq][NW] | Ray[sq][SE] | Ray[sq][SW];
}

static U64 rook_mask(Square sq) {
    return Ray[sq][N] | Ray[sq][S] | Ray[sq][E] | Ray[sq][W];
}

// PEXT sliding attack tables

struct PextSlider { U64 mask; U64 *attacks; };

static PextSlider Bishop[64];
static PextSlider Rook[64];

static U64 BishopTable[524288];
static U64 RookTable[4096000];

static U64 sliding_attacks(Square sq, U64 occ, bool bishop) {
    const int df[8] = { 0,  0, 1, -1,  1, -1,  1, -1 };
    const int dr[8] = { 1, -1, 0,  0,  1,  1, -1, -1 };
    int start = bishop ? 4 : 0;
    int end   = bishop ? 8 : 4;
    int f = get_file(sq), r = get_rank(sq);
    U64 attacks = 0;
    for (int d = start; d < end; ++d) {
        int nf = f + df[d], nr = r + dr[d];
        while (is_valid_square(nf, nr)) {
            U64 bb = bb_square(make_square(File(nf), Rank(nr)));
            attacks |= bb;
            if (occ & bb) break;
            nf += df[d]; nr += dr[d];
        }
    }
    return attacks;
}

void init_attacks() {
    init_rays();

    U64 *b_ptr = BishopTable;
    U64 *r_ptr = RookTable;

    for (int sq = 0; sq < 64; ++sq) {
        U64 b_mask = bishop_mask(Square(sq));
        int b_bits = __builtin_popcountll(b_mask);
        Bishop[sq].mask    = b_mask;
        Bishop[sq].attacks = b_ptr;
        for (U64 occ = 0;; occ = (occ - b_mask) & b_mask) {
            Bishop[sq].attacks[_pext_u64(occ, b_mask)] = sliding_attacks(Square(sq), occ, true);
            if (occ == b_mask) break;
        }
        b_ptr += (1ULL << b_bits);

        U64 r_mask = rook_mask(Square(sq));
        int r_bits = __builtin_popcountll(r_mask);
        Rook[sq].mask    = r_mask;
        Rook[sq].attacks = r_ptr;
        for (U64 occ = 0;; occ = (occ - r_mask) & r_mask) {
            Rook[sq].attacks[_pext_u64(occ, r_mask)] = sliding_attacks(Square(sq), occ, false);
            if (occ == r_mask) break;
        }
        r_ptr += (1ULL << r_bits);
    }
}

U64 bishop_attacks(Square sq, U64 occupied) {
    return Bishop[sq].attacks[_pext_u64(occupied, Bishop[sq].mask)];
}

U64 rook_attacks(Square sq, U64 occupied) {
    return Rook[sq].attacks[_pext_u64(occupied, Rook[sq].mask)];
}

U64 queen_attacks(Square sq, U64 occupied) {
    return bishop_attacks(sq, occupied) | rook_attacks(sq, occupied);
}

Square king_square(const Board &b, Colour c) {
    U64 kbb = (c == WHITE) ? b.bit_boards[WK] : b.bit_boards[BK];
    assert(kbb && "king missing");
    return __builtin_ctzll(kbb);
}

bool is_square_attacked(const Board &b, Square sq, Colour attacker) {
    int f = get_file(sq), r = get_rank(sq);

    if (attacker == WHITE) {
        if (r > 0) {
            if (f > 0 && (b.bit_boards[WP] & bb_square(make_square(File(f-1), Rank(r-1))))) return true;
            if (f < 7 && (b.bit_boards[WP] & bb_square(make_square(File(f+1), Rank(r-1))))) return true;
        }
    } else {
        if (r < 7) {
            if (f > 0 && (b.bit_boards[BP] & bb_square(make_square(File(f-1), Rank(r+1))))) return true;
            if (f < 7 && (b.bit_boards[BP] & bb_square(make_square(File(f+1), Rank(r+1))))) return true;
        }
    }

    U64 kn = knight_attacks(sq);
    if (attacker == WHITE ? (kn & b.bit_boards[WN]) : (kn & b.bit_boards[BN])) return true;

    U64 k = king_attacks(sq);
    if (attacker == WHITE ? (k & b.bit_boards[WK]) : (k & b.bit_boards[BK])) return true;

    U64 ba = bishop_attacks(sq, b.occupied);
    if (attacker == WHITE ? (ba & (b.bit_boards[WB] | b.bit_boards[WQ]))
                          : (ba & (b.bit_boards[BB] | b.bit_boards[BQ]))) return true;

    U64 ra = rook_attacks(sq, b.occupied);
    if (attacker == WHITE ? (ra & (b.bit_boards[WR] | b.bit_boards[WQ]))
                          : (ra & (b.bit_boards[BR] | b.bit_boards[BQ]))) return true;

    return false;
}

} // namespace SHAYVERI
