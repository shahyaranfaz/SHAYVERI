#include "evaluate.h"
#include "attacks.h"
#include "types.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace {
// Piece values (centipawns) — unsigned/absolute for both colours.
// The sign (+/-) is applied in the evaluation loop via `sign = (c == WHITE) ? 1 : -1`.
static constexpr int PIECE_VALUES_MG[PIECE_COUNT] = {
    0,    // NONE_PIECE
    100,  // WP
    320,  // WN
    330,  // WB
    500,  // WR
    900,  // WQ
    0,    // WK
    100,  // BP
    320,  // BN
    330,  // BB
    500,  // BR
    900,  // BQ
    0,    // BK
};

static constexpr int PIECE_VALUES_EG[PIECE_COUNT] = {
    0,    // NONE_PIECE
    100,  // WP
    310,  // WN
    330,  // WB
    510,  // WR
    900,  // WQ
    0,    // WK
    100,  // BP
    310,  // BN
    330,  // BB
    510,  // BR
    900,  // BQ
    0,    // BK
};

// Piece-square tables (white's perspective)
static constexpr int PST_PAWN_MG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0,
};

static constexpr int PST_PAWN_EG[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    10, 15, 15, -5, -5, 15, 15, 10,
    10,  0,  0, 10, 10,  0,  0, 10,
     5,  5, 10, 20, 20, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    20, 20, 30, 40, 40, 30, 20, 20,
    60, 60, 60, 60, 60, 60, 60, 60,
     0,  0,  0,  0,  0,  0,  0,  0,
};

static constexpr int PST_KNIGHT_MG[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

static constexpr int PST_KNIGHT_EG[64] = {
    -40,-30,-20,-20,-20,-20,-30,-40,
    -30,-10,  0,  5,  5,  0,-10,-30,
    -20,  5, 10, 15, 15, 10,  5,-20,
    -20,  0, 15, 20, 20, 15,  0,-20,
    -20,  5, 15, 20, 20, 15,  5,-20,
    -20,  0, 10, 15, 15, 10,  0,-20,
    -30,-10,  0,  0,  0,  0,-10,-30,
    -40,-30,-20,-20,-20,-20,-30,-40,
};

static constexpr int PST_BISHOP_MG[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

static constexpr int PST_BISHOP_EG[64] = {
    -15,-10,-10,-10,-10,-10,-10,-15,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -15,-10,-10,-10,-10,-10,-10,-15,
};

static constexpr int PST_ROOK_MG[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0,
};

static constexpr int PST_ROOK_EG[64] = {
     5,  5,  5, 10, 10,  5,  5,  5,
     0,  0,  0,  5,  5,  0,  0,  0,
     0,  0,  0,  5,  5,  0,  0,  0,
     0,  0,  0,  5,  5,  0,  0,  0,
     0,  0,  0,  5,  5,  0,  0,  0,
     0,  0,  0,  5,  5,  0,  0,  0,
    10, 10, 10, 15, 15, 10, 10, 10,
     5,  5,  5, 10, 10,  5,  5,  5,
};

static constexpr int PST_QUEEN_MG[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
      0,  0,  5,  5,  5,  5,  0, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20,
};

// Endgame queen: penalise rim less harshly and reward central/active squares more.
// No longer a copy of PST_QUEEN_MG.
static constexpr int PST_QUEEN_EG[64] = {
    -10, -5, -5, -5, -5, -5, -5,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
     -5,  5, 10, 10, 10, 10,  5, -5,
     -5,  0, 10, 15, 15, 10,  0, -5,
     -5,  0, 10, 15, 15, 10,  0, -5,
     -5,  5, 10, 10, 10, 10,  5, -5,
     -5,  0,  5,  5,  5,  5,  0, -5,
    -10, -5, -5, -5, -5, -5, -5,-10,
};

static constexpr int PST_KING_MG[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
};

static constexpr int PST_KING_EG[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,
    -30,-30,  0,  0,  0,  0,-30,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 30, 40, 40, 30,-10,-30,
    -30,-10, 20, 30, 30, 20,-10,-30,
    -30,-20,-10,  0,  0,-10,-20,-30,
    -50,-40,-30,-20,-20,-30,-40,-50,
};

static constexpr int PHASE_WEIGHTS[PIECE_COUNT] = {
    0, // none
    0, // pawn
    1, // knight
    1, // bishop
    2, // rook
    4, // queen
    0, // king
    0, // pawn
    1, // knight
    1, // bishop
    2, // rook
    4, // queen
    0  // king
};

static constexpr int MAX_PHASE = 24;

static constexpr int TEMPO_BONUS = 8;
static constexpr int BISHOP_PAIR_BONUS = 30;

static constexpr int PASSED_PAWN_BONUS_MG[8] = { 0, 5, 10, 20, 30, 50, 70, 0 };
static constexpr int PASSED_PAWN_BONUS_EG[8] = { 0, 10, 20, 40, 60, 90, 120, 0 };
static constexpr int CANDIDATE_PAWN_BONUS_MG = 8;
static constexpr int CANDIDATE_PAWN_BONUS_EG = 12;
static constexpr int CONNECTED_PASSED_BONUS_MG = 10;
static constexpr int CONNECTED_PASSED_BONUS_EG = 18;
static constexpr int OUTSIDE_PASSED_BONUS_MG = 8;
static constexpr int OUTSIDE_PASSED_BONUS_EG = 20;
static constexpr int ISOLATED_PAWN_PENALTY_MG = -12;
static constexpr int ISOLATED_PAWN_PENALTY_EG = -8;
static constexpr int DOUBLED_PAWN_PENALTY_MG = -14;
static constexpr int DOUBLED_PAWN_PENALTY_EG = -10;
static constexpr int BACKWARD_PAWN_PENALTY_MG = -10;
static constexpr int BACKWARD_PAWN_PENALTY_EG = -6;
static constexpr int SUPPORTED_PAWN_BONUS_MG = 6;
static constexpr int WEAK_PAWN_PENALTY_MG = -8;
static constexpr int PAWN_ISLAND_PENALTY_MG = -10;
static constexpr int PAWN_ISLAND_PENALTY_EG = -6;
static constexpr int PAWN_CENTER_BONUS_MG = 6;
static constexpr int PAWN_CENTER_BONUS_EG = 2;
static constexpr int PAWN_EXT_CENTER_BONUS_MG = 3;
static constexpr int PAWN_EXT_CENTER_BONUS_EG = 1;
static constexpr int PAWN_STORM_BASE = 4;
static constexpr int PAWN_STORM_RANK_MULT = 2;

static constexpr int KING_SHIELD_MISSING_PENALTY = -14;
static constexpr int KING_OPEN_FILE_PENALTY = -10;
static constexpr int KING_SEMI_OPEN_FILE_PENALTY = -6;
static constexpr int KING_ATTACK_PENALTY = -3;
static constexpr int KING_ESCAPE_BONUS = 4;
static constexpr int OPEN_FILE_MULTIPLIER = 120;
static constexpr int SEMI_OPEN_FILE_MULTIPLIER = 110;
static constexpr int CLOSED_FILE_MULTIPLIER = 100;
static constexpr int BISHOP_OPENNESS_BASE = 100;
static constexpr int BISHOP_OPENNESS_MAX_BONUS = 30;
static constexpr int BISHOP_OPENNESS_SQUARE_WEIGHT = 2;

static constexpr int MOBILITY_KNIGHT_MG = 4;
static constexpr int MOBILITY_BISHOP_MG = 4;
static constexpr int MOBILITY_ROOK_MG = 2;
static constexpr int MOBILITY_QUEEN_MG = 1;

static constexpr int MOBILITY_KNIGHT_EG = 3;
static constexpr int MOBILITY_BISHOP_EG = 4;
static constexpr int MOBILITY_ROOK_EG = 3;
static constexpr int MOBILITY_QUEEN_EG = 2;

static constexpr int CENTER_BONUS = 8;
static constexpr int EXT_CENTER_BONUS = 4;
static constexpr int ENEMY_HALF_BONUS = 3;
static constexpr int SEVENTH_RANK_BONUS_MG = 20;
static constexpr int SEVENTH_RANK_BONUS_EG = 30;

static constexpr int DEFENDED_PIECE_BONUS = 4;
static constexpr int SHARED_TARGET_BONUS = 8;
static constexpr int BATTERY_ROOK_QUEEN_BONUS = 12;
static constexpr int BATTERY_BISHOP_QUEEN_BONUS = 8;
static constexpr int SUPPORT_CHAIN_BONUS = 6;

static constexpr int UNDEFENDED_ATTACK_BONUS = 6;
static constexpr int PIN_BONUS = 15;
static constexpr int OVERLOADED_DEFENDER_BONUS = 10;
static constexpr int UNRECIPROCATED_PRESSURE_BONUS = 2;
static constexpr int UNDEFENDED_VALUE_DIVISOR = 40;

static constexpr int KNIGHT_OUTPOST_MG = 20;
static constexpr int KNIGHT_OUTPOST_EG = 14;
static constexpr int BISHOP_OUTPOST_MG = 12;
static constexpr int BISHOP_OUTPOST_EG = 8;
static constexpr int ROOK_OUTPOST_MG = 14;
static constexpr int ROOK_OUTPOST_EG = 12;
static constexpr int QUEEN_OUTPOST_MG = 10;
static constexpr int QUEEN_OUTPOST_EG = 8;

static constexpr int DEVELOPMENT_BONUS = 5;
static constexpr int CASTLED_BONUS = 12;

// Wrapper for consistent popcount usage in evaluation.
static int popcount(U64 bb) { return __builtin_popcountll(bb); }

static int mirror(int sq) { return (7 - sq / 8) * 8 + (sq % 8); }

static const std::array<U64, 8> FILE_MASKS = [] {
    std::array<U64, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        U64 mask = 0;
        for (int r = 0; r < 8; ++r) mask |= 1ULL << (r * 8 + f);
        masks[f] = mask;
    }
    return masks;
}();

static const std::array<U64, 8> RANK_MASKS = [] {
    std::array<U64, 8> masks{};
    for (int r = 0; r < 8; ++r) {
        U64 mask = 0;
        for (int f = 0; f < 8; ++f) mask |= 1ULL << (r * 8 + f);
        masks[r] = mask;
    }
    return masks;
}();

static const std::array<U64, 8> ADJ_FILE_MASKS = [] {
    std::array<U64, 8> masks{};
    for (int f = 0; f < 8; ++f) {
        U64 mask = 0;
        if (f > 0) mask |= FILE_MASKS[f - 1];
        if (f < 7) mask |= FILE_MASKS[f + 1];
        masks[f] = mask;
    }
    return masks;
}();

static const std::array<std::array<U64, 8>, 2> FORWARD_RANK_MASKS = [] {
    std::array<std::array<U64, 8>, 2> masks{};
    for (int r = 0; r < 8; ++r) {
        U64 white_mask = 0;
        U64 black_mask = 0;
        for (int rr = r + 1; rr < 8; ++rr) white_mask |= RANK_MASKS[rr];
        for (int rr = 0; rr < r; ++rr) black_mask |= RANK_MASKS[rr];
        masks[WHITE][r] = white_mask;
        masks[BLACK][r] = black_mask;
    }
    return masks;
}();

// True centre: d4, e4, d5, e5 (files 3&4, ranks 3&4 in 0-indexed)
static const U64 CENTER_MASK =
    (1ULL << (3 * 8 + 3)) | (1ULL << (3 * 8 + 4)) |
    (1ULL << (4 * 8 + 3)) | (1ULL << (4 * 8 + 4));
// Extended centre: c3-f3, c4-f4, c5-f5, c6-f6 minus the inner 4
static const U64 EXT_CENTER_MASK =
    ((1ULL << (2*8+2))|(1ULL << (2*8+3))|(1ULL << (2*8+4))|(1ULL << (2*8+5)) |
     (1ULL << (3*8+2))|(1ULL << (3*8+5)) |
     (1ULL << (4*8+2))|(1ULL << (4*8+5)) |
     (1ULL << (5*8+2))|(1ULL << (5*8+3))|(1ULL << (5*8+4))|(1ULL << (5*8+5)));

struct AttackInfo {
    std::array<int, 64> counts{};
    std::array<int, 64> non_pawn_counts{};
    U64 all = 0;
    U64 pawn = 0;
    U64 non_pawn = 0;
};

static void add_attacks(AttackInfo &info, U64 attacks, bool non_pawn) {
    info.all |= attacks;
    if (non_pawn) info.non_pawn |= attacks;
    while (attacks) {
        Square sq = pop_lsb(attacks);
        info.counts[sq] += 1;
        if (non_pawn) info.non_pawn_counts[sq] += 1;
    }
}

static AttackInfo build_attack_info(const Board &b, Colour c) {
    AttackInfo info;
    U64 pawns = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    while (pawns) {
        Square sq = pop_lsb(pawns);
        U64 att = pawn_attacks(c, sq);
        info.pawn |= att;
        add_attacks(info, att, false);
    }

    U64 knights = (c == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    while (knights) {
        Square sq = pop_lsb(knights);
        U64 att = knight_attacks(sq);
        add_attacks(info, att, true);
    }

    U64 bishops = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    while (bishops) {
        Square sq = pop_lsb(bishops);
        U64 att = bishop_attacks(sq, b.occupied);
        add_attacks(info, att, true);
    }

    U64 rooks = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    while (rooks) {
        Square sq = pop_lsb(rooks);
        U64 att = rook_attacks(sq, b.occupied);
        add_attacks(info, att, true);
    }

    U64 queens = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];
    while (queens) {
        Square sq = pop_lsb(queens);
        U64 att = queen_attacks(sq, b.occupied);
        add_attacks(info, att, true);
    }

    Square ksq = king_square(b, c);
    add_attacks(info, king_attacks(ksq), true);

    return info;
}

static int phase_score(const Board &b) {
    int phase = 0;
    for (int p = 1; p < PIECE_COUNT; ++p) {
        phase += PHASE_WEIGHTS[p] * popcount(b.bit_boards[p]);
    }
    return std::min(phase, MAX_PHASE);
}

static int pst_mg(PieceType pt, int sq) {
    switch (pt) {
        case PAWN: return PST_PAWN_MG[sq];
        case KNIGHT: return PST_KNIGHT_MG[sq];
        case BISHOP: return PST_BISHOP_MG[sq];
        case ROOK: return PST_ROOK_MG[sq];
        case QUEEN: return PST_QUEEN_MG[sq];
        case KING: return PST_KING_MG[sq];
        default: return 0;
    }
}

static int pst_eg(PieceType pt, int sq) {
    switch (pt) {
        case PAWN: return PST_PAWN_EG[sq];
        case KNIGHT: return PST_KNIGHT_EG[sq];
        case BISHOP: return PST_BISHOP_EG[sq];
        case ROOK: return PST_ROOK_EG[sq];
        case QUEEN: return PST_QUEEN_EG[sq];
        case KING: return PST_KING_EG[sq];
        default: return 0;
    }
}

static void add_score(int &mg, int &eg, int mg_delta, int eg_delta, Colour c) {
    int sign = (c == WHITE) ? 1 : -1;
    mg += sign * mg_delta;
    eg += sign * eg_delta;
}

static bool is_passed_pawn(Colour c, Square sq, U64 enemy_pawns) {
    int f = get_file(sq);
    int r = get_rank(sq);
    U64 forward = FORWARD_RANK_MASKS[c][r];
    U64 files = FILE_MASKS[f] | ADJ_FILE_MASKS[f];
    return (enemy_pawns & forward & files) == 0;
}

static bool has_clear_wing(int file, U64 enemy_pawns) {
    if (file <= FILE_B) {
        U64 wing = FILE_MASKS[FILE_A] | FILE_MASKS[FILE_B] | FILE_MASKS[FILE_C];
        return (enemy_pawns & wing) == 0;
    }
    if (file >= FILE_G) {
        U64 wing = FILE_MASKS[FILE_F] | FILE_MASKS[FILE_G] | FILE_MASKS[FILE_H];
        return (enemy_pawns & wing) == 0;
    }
    return false;
}

static bool is_supported_pawn(Colour c, Square sq, U64 friendly_pawns) {
    int f = get_file(sq);
    int r = get_rank(sq);
    if (c == WHITE) {
        if (r == 0) return false;
        if (f > 0 && (friendly_pawns & bb_square(make_square(File(f - 1), Rank(r - 1))))) return true;
        if (f < 7 && (friendly_pawns & bb_square(make_square(File(f + 1), Rank(r - 1))))) return true;
    } else {
        if (r == 7) return false;
        if (f > 0 && (friendly_pawns & bb_square(make_square(File(f - 1), Rank(r + 1))))) return true;
        if (f < 7 && (friendly_pawns & bb_square(make_square(File(f + 1), Rank(r + 1))))) return true;
    }
    return false;
}

static bool is_backward_pawn(Colour c, Square sq, U64 friendly_pawns, U64 enemy_pawn_attacks, U64 occupied) {
    int f = get_file(sq);
    int r = get_rank(sq);
    U64 adjacent = ADJ_FILE_MASKS[f];
    U64 same_or_forward = 0;
    if (c == WHITE) {
        for (int rr = r; rr < 8; ++rr) same_or_forward |= RANK_MASKS[rr];
    } else {
        for (int rr = 0; rr <= r; ++rr) same_or_forward |= RANK_MASKS[rr];
    }
    bool has_support_ahead = (friendly_pawns & adjacent & same_or_forward) != 0;
    Square forward = SQ_NONE;
    if (c == WHITE && r < 7) forward = sq + 8;
    if (c == BLACK && r > 0) forward = sq - 8;
    bool blocked = (forward != SQ_NONE) && (occupied & bb_square(forward));
    bool controlled = (forward != SQ_NONE) && (enemy_pawn_attacks & bb_square(forward));
    return !has_support_ahead && (blocked || controlled);
}

static void evaluate_pawns(const Board &b, Colour c, const AttackInfo &enemy_attacks, int &mg, int &eg) {
    U64 pawns = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    U64 enemy_pawns = (c == WHITE) ? b.bit_boards[BP] : b.bit_boards[WP];

    std::array<int, 8> file_counts{};
    std::array<bool, 64> passed{};
    U64 passed_bb = 0;

    U64 temp = pawns;
    while (temp) {
        Square sq = pop_lsb(temp);
        int f = get_file(sq);
        file_counts[f] += 1;
        if (is_passed_pawn(c, sq, enemy_pawns)) {
            passed[sq] = true;
            passed_bb |= bb_square(sq);
        }
    }

    int islands = 0;
    bool in_island = false;
    for (int f = 0; f < 8; ++f) {
        if (file_counts[f] > 0) {
            if (!in_island) {
                islands += 1;
                in_island = true;
            }
        } else {
            in_island = false;
        }
    }
    if (islands > 1) {
        add_score(mg, eg, (islands - 1) * PAWN_ISLAND_PENALTY_MG, (islands - 1) * PAWN_ISLAND_PENALTY_EG, c);
    }

    for (int f = 0; f < 8; ++f) {
        if (file_counts[f] > 1) {
            int doubled = file_counts[f] - 1;
            add_score(mg, eg, doubled * DOUBLED_PAWN_PENALTY_MG, doubled * DOUBLED_PAWN_PENALTY_EG, c);
        }
    }

    temp = pawns;
    while (temp) {
        Square sq = pop_lsb(temp);
        int f = get_file(sq);
        int r = get_rank(sq);
        int rel_rank = (c == WHITE) ? r : 7 - r;

        bool isolated = (pawns & ADJ_FILE_MASKS[f]) == 0;
        if (isolated) {
            add_score(mg, eg, ISOLATED_PAWN_PENALTY_MG, ISOLATED_PAWN_PENALTY_EG, c);
        }

        bool supported = is_supported_pawn(c, sq, pawns);
        if (supported) {
            add_score(mg, eg, SUPPORTED_PAWN_BONUS_MG, 0, c);
        }

        bool weak = (enemy_attacks.pawn & bb_square(sq)) && !supported;
        if (weak) {
            add_score(mg, eg, WEAK_PAWN_PENALTY_MG, 0, c);
        }

        if (passed[sq]) {
            add_score(mg, eg, PASSED_PAWN_BONUS_MG[rel_rank], PASSED_PAWN_BONUS_EG[rel_rank], c);
            if (has_clear_wing(f, enemy_pawns)) {
                add_score(mg, eg, OUTSIDE_PASSED_BONUS_MG, OUTSIDE_PASSED_BONUS_EG, c);
            }
            if (passed_bb & ADJ_FILE_MASKS[f]) {
                add_score(mg, eg, CONNECTED_PASSED_BONUS_MG, CONNECTED_PASSED_BONUS_EG, c);
            }
        } else {
            Square forward = SQ_NONE;
            if (c == WHITE && r < 7) forward = sq + 8;
            if (c == BLACK && r > 0) forward = sq - 8;
            bool forward_empty = (forward != SQ_NONE) && !(b.occupied & bb_square(forward));
            if (forward_empty) {
                U64 forward_mask = FORWARD_RANK_MASKS[c][r];
                bool same_file_clear = (enemy_pawns & FILE_MASKS[f] & forward_mask) == 0;
                int enemy_adjacent = popcount(enemy_pawns & ADJ_FILE_MASKS[f] & forward_mask);
                if (same_file_clear && enemy_adjacent <= 1) {
                    add_score(mg, eg, CANDIDATE_PAWN_BONUS_MG, CANDIDATE_PAWN_BONUS_EG, c);
                }
            }
            if (is_backward_pawn(c, sq, pawns, enemy_attacks.pawn, b.occupied)) {
                add_score(mg, eg, BACKWARD_PAWN_PENALTY_MG, BACKWARD_PAWN_PENALTY_EG, c);
            }
        }

        if (bb_square(sq) & CENTER_MASK) {
            add_score(mg, eg, PAWN_CENTER_BONUS_MG, PAWN_CENTER_BONUS_EG, c);
        } else if (bb_square(sq) & EXT_CENTER_MASK) {
            add_score(mg, eg, PAWN_EXT_CENTER_BONUS_MG, PAWN_EXT_CENTER_BONUS_EG, c);
        }
    }

    Square enemy_king = king_square(b, flip(c));
    int king_file = get_file(enemy_king);
    temp = pawns;
    while (temp) {
        Square sq = pop_lsb(temp);
        int f = get_file(sq);
        int r = get_rank(sq);
        int file_dist = std::abs(f - king_file);
        if (file_dist <= 1) {
            if ((c == WHITE && r >= 3) || (c == BLACK && r <= 4)) {
                int advance = (c == WHITE) ? r : 7 - r;
                int storm_bonus = PAWN_STORM_BASE + advance * PAWN_STORM_RANK_MULT;
                add_score(mg, eg, storm_bonus, 0, c);
            }
        }
    }
}

static void mobility_bonus(int count, int mg_weight, int eg_weight, int openness_pct, int &mg, int &eg, Colour c) {
    int mg_delta = count * mg_weight * openness_pct / 100;
    int eg_delta = count * eg_weight * openness_pct / 100;
    add_score(mg, eg, mg_delta, eg_delta, c);
}

static void evaluate_king_safety(const Board &b, Colour c, const AttackInfo &enemy_attacks, int phase, int &mg, int &eg) {
    Square ksq = king_square(b, c);
    int f = get_file(ksq);
    U64 pawns = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    U64 all_pawns = b.bit_boards[WP] | b.bit_boards[BP];

    int shield_rank = (c == WHITE) ? 1 : 6;
    for (int df = -1; df <= 1; ++df) {
        int file = f + df;
        if (file < 0 || file > 7) continue;
        Square shield_sq = make_square(File(file), Rank(shield_rank));
        if (!(pawns & bb_square(shield_sq))) {
            add_score(mg, eg, KING_SHIELD_MISSING_PENALTY, 0, c);
        }
    }

    for (int df = -1; df <= 1; ++df) {
        int file = f + df;
        if (file < 0 || file > 7) continue;
        U64 file_mask = FILE_MASKS[file];
        bool has_friendly = (pawns & file_mask) != 0;
        bool has_any = (all_pawns & file_mask) != 0;
        // Mutually exclusive: open > semi-open (open file already implies no friendly pawn).
        if (!has_any) {
            add_score(mg, eg, KING_OPEN_FILE_PENALTY, 0, c);
        } else if (!has_friendly) {
            add_score(mg, eg, KING_SEMI_OPEN_FILE_PENALTY, 0, c);
        }
    }

    U64 king_zone = king_attacks(ksq) | bb_square(ksq);
    int pressure = popcount(enemy_attacks.all & king_zone);
    add_score(mg, eg, pressure * KING_ATTACK_PENALTY, 0, c);

    // Escape squares: small flat bonus in MG (king needs some mobility even there),
    // larger phase-scaled bonus in EG (active king is crucial in endings).
    int escape = popcount(king_attacks(ksq) & ~b.occupancies[c] & ~enemy_attacks.all);
    int mg_escape = escape * KING_ESCAPE_BONUS / 2;
    int eg_escape = escape * KING_ESCAPE_BONUS * (MAX_PHASE - phase) / MAX_PHASE;
    add_score(mg, eg, mg_escape, eg_escape, c);
}

static int openness_multiplier_for_file(U64 friendly_pawns, U64 all_pawns, int file) {
    U64 mask = FILE_MASKS[file];
    bool has_friendly = (friendly_pawns & mask) != 0;
    bool has_any = (all_pawns & mask) != 0;
    if (!has_any) return OPEN_FILE_MULTIPLIER;
    if (!has_friendly) return SEMI_OPEN_FILE_MULTIPLIER;
    return CLOSED_FILE_MULTIPLIER;
}

static int bishop_openness_multiplier(U64 attacks) {
    int open_squares = popcount(attacks);
    return BISHOP_OPENNESS_BASE + std::min(BISHOP_OPENNESS_MAX_BONUS, open_squares * BISHOP_OPENNESS_SQUARE_WEIGHT);
}

static void evaluate_piece_activity(const Board &b, Colour c, int &mg, int &eg) {
    U64 friendly = b.occupancies[c];
    U64 pawns = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    U64 all_pawns = b.bit_boards[WP] | b.bit_boards[BP];

    // Safe-mobility mask: exclude squares controlled by enemy pawns (fix #5).
    U64 enemy_pawns_bb = (c == WHITE) ? b.bit_boards[BP] : b.bit_boards[WP];
    U64 enemy_pawn_attacks = 0;
    {
        U64 ep = enemy_pawns_bb;
        while (ep) {
            Square s = pop_lsb(ep);
            enemy_pawn_attacks |= pawn_attacks(flip(c), s);
        }
    }
    U64 safe = ~friendly & ~enemy_pawn_attacks;

    U64 knights = (c == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    U64 bishops = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    U64 rooks = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    U64 queens = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];

    U64 temp = knights;
    while (temp) {
        Square sq = pop_lsb(temp);
        U64 attacks = knight_attacks(sq) & safe;
        mobility_bonus(popcount(attacks), MOBILITY_KNIGHT_MG, MOBILITY_KNIGHT_EG, 100, mg, eg, c);
        if (bb_square(sq) & CENTER_MASK) add_score(mg, eg, CENTER_BONUS, CENTER_BONUS / 2, c);
        else if (bb_square(sq) & EXT_CENTER_MASK) add_score(mg, eg, EXT_CENTER_BONUS, EXT_CENTER_BONUS / 2, c);
        if ((c == WHITE && get_rank(sq) >= 4) || (c == BLACK && get_rank(sq) <= 3)) {
            add_score(mg, eg, ENEMY_HALF_BONUS, ENEMY_HALF_BONUS, c);
        }
    }

    temp = bishops;
    while (temp) {
        Square sq = pop_lsb(temp);
        U64 attacks = bishop_attacks(sq, b.occupied) & safe;
        int mult = bishop_openness_multiplier(attacks);
        mobility_bonus(popcount(attacks), MOBILITY_BISHOP_MG, MOBILITY_BISHOP_EG, mult, mg, eg, c);
        if (bb_square(sq) & CENTER_MASK) add_score(mg, eg, CENTER_BONUS, CENTER_BONUS / 2, c);
        else if (bb_square(sq) & EXT_CENTER_MASK) add_score(mg, eg, EXT_CENTER_BONUS, EXT_CENTER_BONUS / 2, c);
        if ((c == WHITE && get_rank(sq) >= 4) || (c == BLACK && get_rank(sq) <= 3)) {
            add_score(mg, eg, ENEMY_HALF_BONUS, ENEMY_HALF_BONUS, c);
        }
    }

    temp = rooks;
    while (temp) {
        Square sq = pop_lsb(temp);
        U64 attacks = rook_attacks(sq, b.occupied) & safe;
        int mult = openness_multiplier_for_file(pawns, all_pawns, get_file(sq));
        mobility_bonus(popcount(attacks), MOBILITY_ROOK_MG, MOBILITY_ROOK_EG, mult, mg, eg, c);
        if ((c == WHITE && get_rank(sq) == RANK_7) || (c == BLACK && get_rank(sq) == RANK_2)) {
            add_score(mg, eg, SEVENTH_RANK_BONUS_MG, SEVENTH_RANK_BONUS_EG, c);
        }
        if ((c == WHITE && get_rank(sq) >= 4) || (c == BLACK && get_rank(sq) <= 3)) {
            add_score(mg, eg, ENEMY_HALF_BONUS, ENEMY_HALF_BONUS, c);
        }
    }

    temp = queens;
    while (temp) {
        Square sq = pop_lsb(temp);
        U64 attacks = queen_attacks(sq, b.occupied) & safe;
        int mult = std::max(openness_multiplier_for_file(pawns, all_pawns, get_file(sq)), bishop_openness_multiplier(attacks));
        mobility_bonus(popcount(attacks), MOBILITY_QUEEN_MG, MOBILITY_QUEEN_EG, mult, mg, eg, c);
        if (bb_square(sq) & CENTER_MASK) add_score(mg, eg, CENTER_BONUS, CENTER_BONUS / 2, c);
        else if (bb_square(sq) & EXT_CENTER_MASK) add_score(mg, eg, EXT_CENTER_BONUS, EXT_CENTER_BONUS / 2, c);
        if ((c == WHITE && get_rank(sq) == RANK_7) || (c == BLACK && get_rank(sq) == RANK_2)) {
            add_score(mg, eg, SEVENTH_RANK_BONUS_MG / 2, SEVENTH_RANK_BONUS_EG / 2, c);
        }
        if ((c == WHITE && get_rank(sq) >= 4) || (c == BLACK && get_rank(sq) <= 3)) {
            add_score(mg, eg, ENEMY_HALF_BONUS, ENEMY_HALF_BONUS, c);
        }
    }
}

static void evaluate_coordination(const Board &b, Colour c, const AttackInfo &attacks, int &mg, int &eg) {
    U64 pieces = b.occupancies[c];
    U64 enemy = b.occupancies[flip(c)];

    U64 temp = pieces;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (attacks.counts[sq] > 0) {
            add_score(mg, eg, DEFENDED_PIECE_BONUS, DEFENDED_PIECE_BONUS, c);
        }
    }

    temp = enemy;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (attacks.counts[sq] >= 2) {
            add_score(mg, eg, SHARED_TARGET_BONUS, SHARED_TARGET_BONUS, c);
        }
    }

    U64 rooks = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    U64 bishops = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    U64 queens = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];

    temp = rooks;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (rook_attacks(sq, b.occupied) & queens) {
            add_score(mg, eg, BATTERY_ROOK_QUEEN_BONUS, BATTERY_ROOK_QUEEN_BONUS, c);
        }
    }

    temp = bishops;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (bishop_attacks(sq, b.occupied) & queens) {
            add_score(mg, eg, BATTERY_BISHOP_QUEEN_BONUS, BATTERY_BISHOP_QUEEN_BONUS, c);
        }
    }

    temp = pieces;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (attacks.non_pawn_counts[sq] > 0) {
            add_score(mg, eg, SUPPORT_CHAIN_BONUS, SUPPORT_CHAIN_BONUS, c);
        }
    }

}

static U64 piece_attacks(Piece p, Square sq, U64 occupied) {
    Colour c = get_colour(p);
    switch (get_type(p)) {
        case PAWN: return pawn_attacks(c, sq);
        case KNIGHT: return knight_attacks(sq);
        case BISHOP: return bishop_attacks(sq, occupied);
        case ROOK: return rook_attacks(sq, occupied);
        case QUEEN: return queen_attacks(sq, occupied);
        case KING: return king_attacks(sq);
        default: return 0;
    }
}

static int count_pins(const Board &b, Colour attacker) {
    Colour defender = flip(attacker);
    Square king = king_square(b, defender);
    int king_f = get_file(king);
    int king_r = get_rank(king);
    int count = 0;

    // is_orthogonal: rooks and queens on rank/file rays.
    // is_diagonal:   bishops and queens on diagonal rays.
    auto check_pin_from = [&](Square from, bool is_orthogonal, bool is_diagonal) {
        int f = get_file(from);
        int r = get_rank(from);
        int df = king_f - f;
        int dr = king_r - r;
        int step_f = 0;
        int step_r = 0;
        if (df == 0 && dr != 0) {
            if (!is_orthogonal) return;
            step_f = 0;
            step_r = (dr > 0) ? 1 : -1;
        } else if (dr == 0 && df != 0) {
            if (!is_orthogonal) return;
            step_f = (df > 0) ? 1 : -1;
            step_r = 0;
        } else if (std::abs(df) == std::abs(dr)) {
            if (!is_diagonal) return;
            step_f = (df > 0) ? 1 : -1;
            step_r = (dr > 0) ? 1 : -1;
        } else {
            return; // not aligned with king at all
        }

        int cf = f + step_f;
        int cr = r + step_r;
        Square pinned_sq = SQ_NONE;
        while ((cf != king_f || cr != king_r) && cf >= 0 && cf < 8 && cr >= 0 && cr < 8) {
            Square sq = make_square(File(cf), Rank(cr));
            Piece p = b.mailbox[sq];
            if (p != NONE_PIECE) {
                if (pinned_sq == SQ_NONE) {
                    if (get_colour(p) == defender && p != (defender == WHITE ? WK : BK))
                        pinned_sq = sq;
                    else
                        return;
                } else {
                    // Hit the second piece. valid pin if king.
                    if (p == (defender == WHITE ? WK : BK)) count += 1;
                    return;
                }
            }
            cf += step_f;
            cr += step_r;
        }
        if (pinned_sq != SQ_NONE) count += 1;
    };

    U64 rooks   = (attacker == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    U64 bishops = (attacker == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    U64 queens  = (attacker == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];

    U64 temp = rooks;
    while (temp) {
        Square sq = pop_lsb(temp);
        check_pin_from(sq, /*orthogonal=*/true, /*diagonal=*/false);
    }

    temp = bishops;
    while (temp) {
        Square sq = pop_lsb(temp);
        check_pin_from(sq, /*orthogonal=*/false, /*diagonal=*/true);
    }

    temp = queens;
    while (temp) {
        Square sq = pop_lsb(temp);
        check_pin_from(sq, /*orthogonal=*/true, /*diagonal=*/true);
    }

    return count;
}

static int count_overloaded_defenders(const Board &b, Colour defender, const AttackInfo &attacker_info) {
    U64 pieces = b.occupancies[defender];
    int count = 0;
    while (pieces) {
        Square sq = pop_lsb(pieces);
        Piece p = b.mailbox[sq];
        U64 defended = piece_attacks(p, sq, b.occupied) & b.occupancies[defender];
        int defended_attacked = 0;
        while (defended) {
            Square dsq = pop_lsb(defended);
            if (attacker_info.counts[dsq] > 0) defended_attacked += 1;
        }
        if (defended_attacked >= 2) count += 1;
    }
    return count;
}

static void evaluate_tactical_pressure(const Board &b, Colour c, const AttackInfo &attacks, const AttackInfo &enemy_attacks, int &mg, int &eg) {
    U64 enemy = b.occupancies[flip(c)];
    U64 temp = enemy;
    while (temp) {
        Square sq = pop_lsb(temp);
        Piece p = b.mailbox[sq];
        if (p == (flip(c) == WHITE ? WK : BK)) continue;
        if (attacks.counts[sq] > 0 && enemy_attacks.counts[sq] == 0) {
            int value = std::max(UNDEFENDED_ATTACK_BONUS, std::abs(PIECE_VALUES_MG[p]) / UNDEFENDED_VALUE_DIVISOR);
            add_score(mg, eg, value, value, c);
        }
    }

    int pins = count_pins(b, c);
    if (pins > 0) {
        add_score(mg, eg, pins * PIN_BONUS, pins * (PIN_BONUS / 2), c);
    }

    int overloaded = count_overloaded_defenders(b, flip(c), attacks);
    if (overloaded > 0) {
        add_score(mg, eg, overloaded * OVERLOADED_DEFENDER_BONUS, overloaded * (OVERLOADED_DEFENDER_BONUS / 2), c);
    }

    int attacks_on_enemy = popcount(attacks.all & enemy);
    int attacks_on_us = popcount(enemy_attacks.all & b.occupancies[c]);
    int pressure = attacks_on_enemy - attacks_on_us;
    if (pressure != 0) {
        add_score(mg, eg, pressure * UNRECIPROCATED_PRESSURE_BONUS, pressure * (UNRECIPROCATED_PRESSURE_BONUS / 2), c);
    }
}

static void evaluate_outposts(const Board &b, Colour c, const AttackInfo &attacks, const AttackInfo &enemy_attacks, int &mg, int &eg) {
    U64 enemy_pawn_attacks = enemy_attacks.pawn;
    U64 friendly_pawn_attacks = attacks.pawn;

    U64 knights = (c == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    U64 bishops = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    U64 rooks = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    U64 queens = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];

    U64 temp = knights;
    while (temp) {
        Square sq = pop_lsb(temp);
        bool on_enemy_half = (c == WHITE) ? get_rank(sq) >= 4 : get_rank(sq) <= 3;
        bool safe = !(enemy_pawn_attacks & bb_square(sq));
        bool supported = friendly_pawn_attacks & bb_square(sq);
        if (on_enemy_half && safe && supported) {
            add_score(mg, eg, KNIGHT_OUTPOST_MG, KNIGHT_OUTPOST_EG, c);
        }
    }

    temp = bishops;
    while (temp) {
        Square sq = pop_lsb(temp);
        bool on_enemy_half = (c == WHITE) ? get_rank(sq) >= 4 : get_rank(sq) <= 3;
        bool safe = !(enemy_pawn_attacks & bb_square(sq));
        bool supported = friendly_pawn_attacks & bb_square(sq);
        if (on_enemy_half && safe && supported) {
            add_score(mg, eg, BISHOP_OUTPOST_MG, BISHOP_OUTPOST_EG, c);
        }
    }

    temp = rooks;
    while (temp) {
        Square sq = pop_lsb(temp);
        bool on_seventh = (c == WHITE) ? get_rank(sq) == RANK_7 : get_rank(sq) == RANK_2;
        int file = get_file(sq);
        int mult = openness_multiplier_for_file((c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP], b.bit_boards[WP] | b.bit_boards[BP], file);
        if (on_seventh || mult > 100) {
            add_score(mg, eg, ROOK_OUTPOST_MG, ROOK_OUTPOST_EG, c);
        }
    }

    temp = queens;
    while (temp) {
        Square sq = pop_lsb(temp);
        bool on_center = (bb_square(sq) & CENTER_MASK) != 0;
        bool safe = !(enemy_pawn_attacks & bb_square(sq));
        if (on_center && safe) {
            add_score(mg, eg, QUEEN_OUTPOST_MG, QUEEN_OUTPOST_EG, c);
        }
    }
}

static int development_score(const Board &b, Colour c, int phase) {
    // Development only meaningful in the opening (high phase = lots of material = early game).
    // Scale linearly to zero as phase drops below half material on the board.
    if (phase <= MAX_PHASE / 2) return 0;

    int developed = 0;
    if (c == WHITE) {
        if (!(b.bit_boards[WN] & bb_square(make_square(FILE_B, RANK_1)))) developed += 1;
        if (!(b.bit_boards[WN] & bb_square(make_square(FILE_G, RANK_1)))) developed += 1;
        if (!(b.bit_boards[WB] & bb_square(make_square(FILE_C, RANK_1)))) developed += 1;
        if (!(b.bit_boards[WB] & bb_square(make_square(FILE_F, RANK_1)))) developed += 1;
    } else {
        if (!(b.bit_boards[BN] & bb_square(make_square(FILE_B, RANK_8)))) developed += 1;
        if (!(b.bit_boards[BN] & bb_square(make_square(FILE_G, RANK_8)))) developed += 1;
        if (!(b.bit_boards[BB] & bb_square(make_square(FILE_C, RANK_8)))) developed += 1;
        if (!(b.bit_boards[BB] & bb_square(make_square(FILE_F, RANK_8)))) developed += 1;
    }

    Square ksq = king_square(b, c);
    bool castled = (c == WHITE)
        ? (ksq == make_square(FILE_G, RANK_1) || ksq == make_square(FILE_C, RANK_1))
        : (ksq == make_square(FILE_G, RANK_8) || ksq == make_square(FILE_C, RANK_8));
    // Scale score proportionally to how far into the opening we are.
    int raw = developed * DEVELOPMENT_BONUS + (castled ? CASTLED_BONUS : 0);
    return raw * (phase - MAX_PHASE / 2) / (MAX_PHASE / 2);
}
}

int evaluate(const Board &b) {
    int mg = 0;
    int eg = 0;

    int phase = phase_score(b);

    AttackInfo white_attacks = build_attack_info(b, WHITE);
    AttackInfo black_attacks = build_attack_info(b, BLACK);

    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = b.bit_boards[p];
        Colour c = get_colour(Piece(p));
        PieceType pt = get_type(Piece(p));
        int sign = (c == WHITE) ? 1 : -1;
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;
            int pst_sq = (c == WHITE) ? sq : mirror(sq);
            mg += sign * (PIECE_VALUES_MG[p] + pst_mg(pt, pst_sq));
            eg += sign * (PIECE_VALUES_EG[p] + pst_eg(pt, pst_sq));
        }
    }

    if (popcount(b.bit_boards[WB]) >= 2) {
        mg += BISHOP_PAIR_BONUS;
        eg += BISHOP_PAIR_BONUS / 2;
    }
    if (popcount(b.bit_boards[BB]) >= 2) {
        mg -= BISHOP_PAIR_BONUS;
        eg -= BISHOP_PAIR_BONUS / 2;
    }

    evaluate_pawns(b, WHITE, black_attacks, mg, eg);
    evaluate_pawns(b, BLACK, white_attacks, mg, eg);

    evaluate_king_safety(b, WHITE, black_attacks, phase, mg, eg);
    evaluate_king_safety(b, BLACK, white_attacks, phase, mg, eg);

    evaluate_piece_activity(b, WHITE, mg, eg);
    evaluate_piece_activity(b, BLACK, mg, eg);

    evaluate_coordination(b, WHITE, white_attacks, mg, eg);
    evaluate_coordination(b, BLACK, black_attacks, mg, eg);

    evaluate_tactical_pressure(b, WHITE, white_attacks, black_attacks, mg, eg);
    evaluate_tactical_pressure(b, BLACK, black_attacks, white_attacks, mg, eg);

    evaluate_outposts(b, WHITE, white_attacks, black_attacks, mg, eg);
    evaluate_outposts(b, BLACK, black_attacks, white_attacks, mg, eg);

    int dev_white = development_score(b, WHITE, phase);
    int dev_black = development_score(b, BLACK, phase);
    int dev_diff = dev_white - dev_black;
    int opening_scale = phase;
    mg += dev_diff * opening_scale / MAX_PHASE;

    int pressure_diff = popcount(white_attacks.all & b.occupancies[BLACK]) - popcount(black_attacks.all & b.occupancies[WHITE]);
    mg += pressure_diff * opening_scale / MAX_PHASE; // Apply pressure advantage objectively
    if (b.side_to_move == WHITE) {
        mg += TEMPO_BONUS; eg += TEMPO_BONUS;
    } else {
        mg -= TEMPO_BONUS; eg -= TEMPO_BONUS;
    }

    int score = (mg * phase + eg * (MAX_PHASE - phase)) / MAX_PHASE;
    return b.side_to_move == WHITE ? score : -score;
}