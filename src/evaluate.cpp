#include "evaluate.h"
#include "attacks.h"
#include "types.h"
#include "tune.h"

#include <array>
#include <algorithm>

namespace SHAYVERI {

using namespace Tune;

namespace {

// PST dispatch tables — reference the inline arrays in tune.h.
static const int* PST_MG_TABLE[7] = {
    nullptr,
    PST_PAWN_MG, PST_KNIGHT_MG, PST_BISHOP_MG,
    PST_ROOK_MG, PST_QUEEN_MG,  PST_KING_MG,
};
static const int* PST_EG_TABLE[7] = {
    nullptr,
    PST_PAWN_EG, PST_KNIGHT_EG, PST_BISHOP_EG,
    PST_ROOK_EG, PST_QUEEN_EG,  PST_KING_EG,
};

// Utilities
// ============================================================
static inline int popcount(U64 bb) { return __builtin_popcountll(bb); }
static inline int mirror(int sq)   { return (7 - sq / 8) * 8 + (sq % 8); }

// Pre-baked masks — computed once at static init.
static const std::array<U64, 8> FILE_MASKS = [] {
    std::array<U64, 8> m{};
    for (int f = 0; f < 8; ++f)
        for (int r = 0; r < 8; ++r)
            m[f] |= 1ULL << (r * 8 + f);
    return m;
}();

static const std::array<U64, 8> RANK_MASKS = [] {
    std::array<U64, 8> m{};
    for (int r = 0; r < 8; ++r)
        for (int f = 0; f < 8; ++f)
            m[r] |= 1ULL << (r * 8 + f);
    return m;
}();

static const std::array<U64, 8> ADJ_FILE_MASKS = [] {
    std::array<U64, 8> m{};
    for (int f = 0; f < 8; ++f) {
        if (f > 0) m[f] |= FILE_MASKS[f - 1];
        if (f < 7) m[f] |= FILE_MASKS[f + 1];
    }
    return m;
}();

static const std::array<std::array<U64, 8>, 2> FORWARD_RANK_MASKS = [] {
    std::array<std::array<U64, 8>, 2> m{};
    for (int r = 0; r < 8; ++r) {
        for (int rr = r + 1; rr < 8; ++rr) m[WHITE][r] |= RANK_MASKS[rr];
        for (int rr = 0;     rr < r;  ++rr) m[BLACK][r] |= RANK_MASKS[rr];
    }
    return m;
}();

static const U64 CENTER_MASK =
    (1ULL << (3*8+3)) | (1ULL << (3*8+4)) |
    (1ULL << (4*8+3)) | (1ULL << (4*8+4));

// ============================================================
// AttackInfo
// Stores per-type attack bitboards so king danger can weight
// attackers by piece type without re-iterating pieces.
// ============================================================
struct AttackInfo {
    U64 by_type[7] = {};          // indexed by PieceType (1=PAWN..6=KING)
    U64 all        = 0;
    U64 pawn       = 0;
    U64 non_pawn   = 0;
    std::array<int, 64> counts{};
    std::array<int, 64> non_pawn_counts{};
};

static void add_attacks(AttackInfo &info, U64 attacks, PieceType pt) {
    info.all         |= attacks;
    info.by_type[pt] |= attacks;
    if (pt == PAWN) {
        info.pawn |= attacks;
    } else {
        info.non_pawn |= attacks;
    }
    U64 tmp = attacks;
    while (tmp) {
        Square sq = pop_lsb(tmp);
        info.counts[sq]++;
        if (pt != PAWN) info.non_pawn_counts[sq]++;
    }
}

static AttackInfo build_attack_info(const Board &b, Colour c) {
    AttackInfo info;

    U64 tmp;

    tmp = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    while (tmp) { Square s = pop_lsb(tmp); add_attacks(info, pawn_attacks(c, s), PAWN); }

    tmp = (c == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    while (tmp) { Square s = pop_lsb(tmp); add_attacks(info, knight_attacks(s), KNIGHT); }

    tmp = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    while (tmp) { Square s = pop_lsb(tmp); add_attacks(info, bishop_attacks(s, b.occupied), BISHOP); }

    tmp = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    while (tmp) { Square s = pop_lsb(tmp); add_attacks(info, rook_attacks(s, b.occupied), ROOK); }

    tmp = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];
    while (tmp) { Square s = pop_lsb(tmp); add_attacks(info, queen_attacks(s, b.occupied), QUEEN); }

    add_attacks(info, king_attacks(king_square(b, c)), KING);

    return info;
}

// ============================================================
// Phase
// ============================================================
static int phase_score(const Board &b) {
    // Unrolled — avoids the PHASE_WEIGHTS loop and is branch-free.
    int phase =
          popcount(b.bit_boards[WN] | b.bit_boards[BN])
        + popcount(b.bit_boards[WB] | b.bit_boards[BB])
        + popcount(b.bit_boards[WR] | b.bit_boards[BR]) * 2
        + popcount(b.bit_boards[WQ] | b.bit_boards[BQ]) * 4;
    return std::min(phase, MAX_PHASE);
}

// ============================================================
// Score accumulation helper
// ============================================================
static inline void add_score(int &mg, int &eg, int mg_d, int eg_d, Colour c) {
    int sign = (c == WHITE) ? 1 : -1;
    mg += sign * mg_d;
    eg += sign * eg_d;
}

// ============================================================
// Pawn helpers
// ============================================================
static bool is_passed_pawn(Colour c, Square sq, U64 enemy_pawns) {
    int f = get_file(sq), r = get_rank(sq);
    U64 forward = FORWARD_RANK_MASKS[c][r];
    U64 files   = FILE_MASKS[f] | ADJ_FILE_MASKS[f];
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
    int f = get_file(sq), r = get_rank(sq);
    if (c == WHITE) {
        if (r == 0) return false;
        if (f > 0 && (friendly_pawns & bb_square(make_square(File(f-1), Rank(r-1))))) return true;
        if (f < 7 && (friendly_pawns & bb_square(make_square(File(f+1), Rank(r-1))))) return true;
    } else {
        if (r == 7) return false;
        if (f > 0 && (friendly_pawns & bb_square(make_square(File(f-1), Rank(r+1))))) return true;
        if (f < 7 && (friendly_pawns & bb_square(make_square(File(f+1), Rank(r+1))))) return true;
    }
    return false;
}

static bool is_backward_pawn(Colour c, Square sq, U64 friendly_pawns,
                              U64 enemy_pawn_attacks, U64 occupied) {
    int f = get_file(sq), r = get_rank(sq);
    U64 adjacent      = ADJ_FILE_MASKS[f];
    U64 same_or_fwd   = 0;
    if (c == WHITE) {
        for (int rr = r; rr < 8; ++rr) same_or_fwd |= RANK_MASKS[rr];
    } else {
        for (int rr = 0; rr <= r; ++rr) same_or_fwd |= RANK_MASKS[rr];
    }
    bool has_support = (friendly_pawns & adjacent & same_or_fwd) != 0;
    Square fwd        = SQ_NONE;
    if (c == WHITE && r < 7) fwd = sq + 8;
    if (c == BLACK && r > 0) fwd = sq - 8;
    bool blocked   = (fwd != SQ_NONE) && (occupied           & bb_square(fwd));
    bool controlled= (fwd != SQ_NONE) && (enemy_pawn_attacks & bb_square(fwd));
    return !has_support && (blocked || controlled);
}

// ============================================================
// Pawn evaluation
// ============================================================
static void evaluate_pawns(const Board &b, Colour c,
                            const AttackInfo &enemy_attacks,
                            int &mg, int &eg) {
    U64 pawns       = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    U64 enemy_pawns = (c == WHITE) ? b.bit_boards[BP] : b.bit_boards[WP];

    std::array<int,  8> file_counts{};
    std::array<bool, 64> passed{};
    U64 passed_bb = 0;

    U64 temp = pawns;
    while (temp) {
        Square sq = pop_lsb(temp);
        file_counts[get_file(sq)]++;
        if (is_passed_pawn(c, sq, enemy_pawns)) {
            passed[sq]  = true;
            passed_bb  |= bb_square(sq);
        }
    }

    // Islands
    int islands   = 0;
    bool in_island = false;
    for (int f = 0; f < 8; ++f) {
        if (file_counts[f] > 0) {
            if (!in_island) { islands++; in_island = true; }
        } else {
            in_island = false;
        }
    }
    if (islands > 1)
        add_score(mg, eg, (islands-1)*PAWN_ISLAND_PENALTY_MG,
                          (islands-1)*PAWN_ISLAND_PENALTY_EG, c);

    // Doubled pawns
    for (int f = 0; f < 8; ++f) {
        if (file_counts[f] > 1) {
            int d = file_counts[f] - 1;
            add_score(mg, eg, d*DOUBLED_PAWN_PENALTY_MG,
                              d*DOUBLED_PAWN_PENALTY_EG, c);
        }
    }

    temp = pawns;
    while (temp) {
        Square sq     = pop_lsb(temp);
        int f         = get_file(sq);
        int r         = get_rank(sq);
        int rel_rank  = (c == WHITE) ? r : 7 - r;
        U64 sq_bb     = bb_square(sq);

        bool isolated = (pawns & ADJ_FILE_MASKS[f]) == 0;
        if (isolated) add_score(mg, eg, ISOLATED_PAWN_PENALTY_MG,
                                        ISOLATED_PAWN_PENALTY_EG, c);

        bool supported = is_supported_pawn(c, sq, pawns);
        if (supported) add_score(mg, eg, SUPPORTED_PAWN_BONUS_MG, SUPPORTED_PAWN_BONUS_EG, c);

        bool weak = (enemy_attacks.pawn & sq_bb) && !supported;
        if (weak) add_score(mg, eg, WEAK_PAWN_PENALTY_MG, WEAK_PAWN_PENALTY_EG, c);

        if (passed[sq]) {
            add_score(mg, eg, PASSED_PAWN_BONUS_MG[rel_rank],
                              PASSED_PAWN_BONUS_EG[rel_rank], c);
            if (has_clear_wing(f, enemy_pawns))
                add_score(mg, eg, OUTSIDE_PASSED_BONUS_MG,
                                  OUTSIDE_PASSED_BONUS_EG, c);
            if (passed_bb & ADJ_FILE_MASKS[f])
                add_score(mg, eg, CONNECTED_PASSED_BONUS_MG,
                                  CONNECTED_PASSED_BONUS_EG, c);
        } else {
            Square fwd = SQ_NONE;
            if (c == WHITE && r < 7) fwd = sq + 8;
            if (c == BLACK && r > 0) fwd = sq - 8;
            bool fwd_empty = (fwd != SQ_NONE) && !(b.occupied & bb_square(fwd));
            if (fwd_empty) {
                U64 fwd_mask   = FORWARD_RANK_MASKS[c][r];
                bool same_clear = (enemy_pawns & FILE_MASKS[f] & fwd_mask) == 0;
                int adj_enemy   = popcount(enemy_pawns & ADJ_FILE_MASKS[f] & fwd_mask);
                if (same_clear && adj_enemy <= 1)
                    add_score(mg, eg, CANDIDATE_PAWN_BONUS_MG,
                                      CANDIDATE_PAWN_BONUS_EG, c);
            }
            if (is_backward_pawn(c, sq, pawns, enemy_attacks.pawn, b.occupied))
                add_score(mg, eg, BACKWARD_PAWN_PENALTY_MG,
                                  BACKWARD_PAWN_PENALTY_EG, c);
        }


    }

    // Pawn storm toward enemy king
    Square enemy_king  = king_square(b, flip(c));
    int    king_file   = get_file(enemy_king);
    temp = pawns;
    while (temp) {
        Square sq = pop_lsb(temp);
        int f = get_file(sq), r = get_rank(sq);
        if (std::abs(f - king_file) <= 1) {
            if ((c == WHITE && r >= 3) || (c == BLACK && r <= 4)) {
                int adv = (c == WHITE) ? r : 7 - r;
                add_score(mg, eg, PAWN_STORM_BASE + adv * PAWN_STORM_RANK_MULT, 0, c);
            }
        }
    }
}

// ============================================================
// King safety — nonlinear danger model
// ============================================================
static void evaluate_king_safety(const Board &b, Colour c,
                                  const AttackInfo &enemy_attacks,
                                  int phase, int &mg, int &eg) {
    Square ksq = king_square(b, c);
    int f      = get_file(ksq);
    U64 pawns      = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    U64 all_pawns  = b.bit_boards[WP] | b.bit_boards[BP];

    // ---- Pawn shield ----
    int shield_rank = (c == WHITE) ? 1 : 6;
    for (int df = -1; df <= 1; ++df) {
        int file = f + df;
        if (file < 0 || file > 7) continue;
        if (!(pawns & bb_square(make_square(File(file), Rank(shield_rank)))))
            add_score(mg, eg, KING_SHIELD_MISSING_PENALTY, 0, c);
    }

    // ---- Open / semi-open files near king ----
    for (int df = -1; df <= 1; ++df) {
        int file = f + df;
        if (file < 0 || file > 7) continue;
        U64 fmask       = FILE_MASKS[file];
        bool has_friend = (pawns      & fmask) != 0;
        bool has_any    = (all_pawns  & fmask) != 0;
        if (!has_any)    add_score(mg, eg, KING_OPEN_FILE_PENALTY,      0, c);
        else if (!has_friend) add_score(mg, eg, KING_SEMI_OPEN_FILE_PENALTY, 0, c);
    }

    // ---- Nonlinear danger accumulation ----
    // King zone = king square + all adjacent squares.
    U64 king_zone = king_attacks(ksq) | bb_square(ksq);

    int danger        = 0;
    int attacker_types = 0;
    for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
        U64 zone_hits = enemy_attacks.by_type[pt] & king_zone;
        if (zone_hits) {
            // Weight by number of squares of the king zone this piece type covers.
            // Two queens both covering the zone is more dangerous than one — intentional.
            danger += KING_ATTACKER_WEIGHT[pt] * popcount(zone_hits);
            attacker_types++;
        }
    }
    // Bonus for having several different attacker types coordinating.
    danger += KING_ATTACK_COUNT_BONUS[std::min(attacker_types, 7)];

    // Quadratic penalty: small dangers are cheap, large dangers are catastrophic.
    // Scaled by phase so it diminishes naturally as pieces come off the board.
    int raw_penalty = std::min(danger * danger / KING_DANGER_DIVISOR, KING_DANGER_MAX);
    int mg_penalty  = raw_penalty * phase / MAX_PHASE;
    int eg_penalty  = raw_penalty / 4;   // residual EG component
    add_score(mg, eg, -mg_penalty, -eg_penalty, c);

    // ---- Escape squares ----
    int escape    = popcount(king_attacks(ksq) & ~b.occupancies[c] & ~enemy_attacks.all);
    int mg_escape = escape * KING_ESCAPE_BONUS / 2;
    int eg_escape = escape * KING_ESCAPE_BONUS * (MAX_PHASE - phase) / MAX_PHASE;
    add_score(mg, eg, mg_escape, eg_escape, c);
}

// ============================================================
// Mobility helpers
// ============================================================
static int openness_multiplier_for_file(U64 friendly_pawns, U64 all_pawns, int file) {
    U64 mask        = FILE_MASKS[file];
    bool has_friend = (friendly_pawns & mask) != 0;
    bool has_any    = (all_pawns       & mask) != 0;
    if (!has_any)    return OPEN_FILE_MULTIPLIER;
    if (!has_friend) return SEMI_OPEN_FILE_MULTIPLIER;
    return CLOSED_FILE_MULTIPLIER;
}

static int bishop_openness_multiplier(U64 attacks) {
    int open_sq = popcount(attacks);
    return BISHOP_OPENNESS_BASE +
           std::min(BISHOP_OPENNESS_MAX_BONUS, open_sq * BISHOP_OPENNESS_SQUARE_WEIGHT);
}

static inline void mobility_bonus(int count, int mg_w, int eg_w,
                                   int openness_pct, int &mg, int &eg, Colour c) {
    add_score(mg, eg,
              count * mg_w * openness_pct / 100,
              count * eg_w * openness_pct / 100, c);
}

// ============================================================
// Piece activity (mobility + territory)
// enemy_attacks is now passed in to avoid recomputing pawn attacks.
// ============================================================
static void evaluate_piece_activity(const Board &b, Colour c,
                                     const AttackInfo &enemy_attacks,
                                     int &mg, int &eg) {
    U64 friendly   = b.occupancies[c];
    U64 pawns      = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    U64 all_pawns  = b.bit_boards[WP] | b.bit_boards[BP];

    // Safe mobility: exclude own pieces and squares controlled by enemy pawns.
    U64 safe = ~friendly & ~enemy_attacks.pawn;

    U64 temp;

    // Knights
    temp = (c == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    while (temp) {
        Square sq = pop_lsb(temp);
        U64 attacks = knight_attacks(sq) & safe;
        mobility_bonus(popcount(attacks), MOBILITY_KNIGHT_MG, MOBILITY_KNIGHT_EG, 100, mg, eg, c);
    }

    // Bishops
    temp = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    while (temp) {
        Square sq   = pop_lsb(temp);
        U64 attacks = bishop_attacks(sq, b.occupied) & safe;
        int mult    = bishop_openness_multiplier(attacks);
        mobility_bonus(popcount(attacks), MOBILITY_BISHOP_MG, MOBILITY_BISHOP_EG, mult, mg, eg, c);
    }

    // Rooks
    temp = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    while (temp) {
        Square sq   = pop_lsb(temp);
        U64 attacks = rook_attacks(sq, b.occupied) & safe;
        int mult    = openness_multiplier_for_file(pawns, all_pawns, get_file(sq));
        mobility_bonus(popcount(attacks), MOBILITY_ROOK_MG, MOBILITY_ROOK_EG, mult, mg, eg, c);
        if ((c == WHITE && get_rank(sq) == RANK_7) || (c == BLACK && get_rank(sq) == RANK_2))
            add_score(mg, eg, SEVENTH_RANK_BONUS_MG, SEVENTH_RANK_BONUS_EG, c);
    }

    // Queens
    temp = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];
    while (temp) {
        Square sq   = pop_lsb(temp);
        U64 attacks = queen_attacks(sq, b.occupied) & safe;
        int mult    = std::max(openness_multiplier_for_file(pawns, all_pawns, get_file(sq)),
                               bishop_openness_multiplier(attacks));
        mobility_bonus(popcount(attacks), MOBILITY_QUEEN_MG, MOBILITY_QUEEN_EG, mult, mg, eg, c);
        if ((c == WHITE && get_rank(sq) == RANK_7) || (c == BLACK && get_rank(sq) == RANK_2))
            add_score(mg, eg, QUEEN_SEVENTH_RANK_BONUS_MG, QUEEN_SEVENTH_RANK_BONUS_EG, c);
    }
}

// ============================================================
// Coordination / connectivity
// ============================================================
static void evaluate_coordination(const Board &b, Colour c,
                                   const AttackInfo &attacks,
                                   int &mg, int &eg) {
    U64 pieces = b.occupancies[c];
    U64 enemy  = b.occupancies[flip(c)];

    // Defended pieces
    U64 temp = pieces;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (attacks.counts[sq] > 0)
            add_score(mg, eg, DEFENDED_PIECE_BONUS_MG, DEFENDED_PIECE_BONUS_EG, c);
    }

    // Shared attack targets
    temp = enemy;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (attacks.counts[sq] >= 2)
            add_score(mg, eg, SHARED_TARGET_BONUS_MG, SHARED_TARGET_BONUS_EG, c);
    }

    // Batteries
    U64 rooks   = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    U64 bishops = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    U64 queens  = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];

    temp = rooks;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (rook_attacks(sq, b.occupied) & queens)
            add_score(mg, eg, BATTERY_ROOK_QUEEN_BONUS_MG, BATTERY_ROOK_QUEEN_BONUS_EG, c);
    }
    temp = bishops;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (bishop_attacks(sq, b.occupied) & queens)
            add_score(mg, eg, BATTERY_BISHOP_QUEEN_BONUS_MG, BATTERY_BISHOP_QUEEN_BONUS_EG, c);
    }

    // Support chains (non-pawn pieces protecting each other)
    temp = pieces;
    while (temp) {
        Square sq = pop_lsb(temp);
        if (attacks.non_pawn_counts[sq] > 0)
            add_score(mg, eg, SUPPORT_CHAIN_BONUS_MG, SUPPORT_CHAIN_BONUS_EG, c);
    }
}

// ============================================================
// Tactical pressure (pins, overloads, unreciprocated attacks)
// ============================================================
static U64 piece_attacks_bb(Piece p, Square sq, U64 occupied) {
    Colour col = get_colour(p);
    switch (get_type(p)) {
        case PAWN:   return pawn_attacks(col, sq);
        case KNIGHT: return knight_attacks(sq);
        case BISHOP: return bishop_attacks(sq, occupied);
        case ROOK:   return rook_attacks(sq, occupied);
        case QUEEN:  return queen_attacks(sq, occupied);
        case KING:   return king_attacks(sq);
        default:     return 0;
    }
}

static int count_pins(const Board &b, Colour attacker) {
    Colour defender = flip(attacker);
    Square king     = king_square(b, defender);
    int king_f      = get_file(king);
    int king_r      = get_rank(king);
    int count       = 0;

    auto check_pin_from = [&](Square from, bool ortho, bool diag) {
        int f = get_file(from), r = get_rank(from);
        int df = king_f - f, dr = king_r - r;
        int sf = 0, sr = 0;
        if      (df == 0 && dr != 0)                       { if (!ortho) return; sr = (dr>0)?1:-1; }
        else if (dr == 0 && df != 0)                       { if (!ortho) return; sf = (df>0)?1:-1; }
        else if (std::abs(df) == std::abs(dr))             { if (!diag)  return; sf=(df>0)?1:-1; sr=(dr>0)?1:-1; }
        else                                                return;

        int cf = f + sf, cr = r + sr;
        Square pinned = SQ_NONE;
        while ((cf != king_f || cr != king_r) && cf >= 0 && cf < 8 && cr >= 0 && cr < 8) {
            Square sq = make_square(File(cf), Rank(cr));
            Piece  p  = b.mailbox[sq];
            if (p != NONE_PIECE) {
                if (pinned == SQ_NONE) {
                    Piece king_piece = (defender == WHITE) ? WK : BK;
                    if (get_colour(p) == defender && p != king_piece) pinned = sq;
                    else return;
                } else {
                    if (p == ((defender == WHITE) ? WK : BK)) count++;
                    return;
                }
            }
            cf += sf; cr += sr;
        }
        if (pinned != SQ_NONE) count++;
    };

    U64 rooks   = (attacker == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    U64 bishops = (attacker == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    U64 queens  = (attacker == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];

    U64 tmp = rooks;
    while (tmp) { Square s = pop_lsb(tmp); check_pin_from(s, true,  false); }
    tmp = bishops;
    while (tmp) { Square s = pop_lsb(tmp); check_pin_from(s, false, true);  }
    tmp = queens;
    while (tmp) { Square s = pop_lsb(tmp); check_pin_from(s, true,  true);  }

    return count;
}

static int count_overloaded_defenders(const Board &b, Colour defender,
                                       const AttackInfo &attacker_info) {
    U64 pieces = b.occupancies[defender];
    int count  = 0;
    while (pieces) {
        Square sq = pop_lsb(pieces);
        Piece p   = b.mailbox[sq];
        U64 defended = piece_attacks_bb(p, sq, b.occupied) & b.occupancies[defender];
        int n = 0;
        U64 tmp = defended;
        while (tmp) {
            Square dsq = pop_lsb(tmp);
            if (attacker_info.counts[dsq] > 0) n++;
        }
        if (n >= 2) count++;
    }
    return count;
}

static void evaluate_tactical_pressure(const Board &b, Colour c,
                                        const AttackInfo &attacks,
                                        const AttackInfo &enemy_attacks,
                                        int &mg, int &eg) {
    U64 enemy = b.occupancies[flip(c)];
    Piece enemy_king_piece = (flip(c) == WHITE) ? WK : BK;

    U64 temp = enemy;
    while (temp) {
        Square sq = pop_lsb(temp);
        Piece p = b.mailbox[sq];
        if (p == enemy_king_piece) continue;
        if (attacks.counts[sq] > 0 && enemy_attacks.counts[sq] == 0) {
            int val = std::max(UNDEFENDED_ATTACK_BONUS,
                               std::abs(PIECE_VALUES_MG[p]) / UNDEFENDED_VALUE_DIVISOR);
            add_score(mg, eg, val, val, c);
        }
    }

    int pins       = count_pins(b, c);
    if (pins > 0)  add_score(mg, eg, pins * PIN_BONUS_MG, pins * PIN_BONUS_EG, c);

    int overloaded = count_overloaded_defenders(b, flip(c), attacks);
    if (overloaded > 0)
        add_score(mg, eg, overloaded * OVERLOADED_DEFENDER_BONUS_MG,
                          overloaded * OVERLOADED_DEFENDER_BONUS_EG, c);

    int atk_on_enemy = popcount(attacks.all     & enemy);
    int atk_on_us    = popcount(enemy_attacks.all & b.occupancies[c]);
    int pressure     = atk_on_enemy - atk_on_us;
    if (pressure != 0)
        add_score(mg, eg, pressure * UNRECIPROCATED_PRESSURE_BONUS_MG,
                          pressure * UNRECIPROCATED_PRESSURE_BONUS_EG, c);
}

// ============================================================
// Threats (new)
//
// Complements tactical_pressure with explicit piece-type-valued bonuses:
//   • Pawn attacks on non-pawn enemy pieces
//   • Minor piece attacks on rooks / queens
//   • Rook attacks on queens
//   • Hanging enemy pieces (attacked by us, zero defenders)
// ============================================================
static void evaluate_threats(const Board &b, Colour c,
                              const AttackInfo &attacks,
                              const AttackInfo &enemy_attacks,
                              int &mg, int &eg) {
    Colour them        = flip(c);
    U64    enemy       = b.occupancies[them];
    Piece  enemy_pawn  = (them == WHITE) ? WP : BP;

    // 1. Pawn threats: our pawns attacking enemy non-pawns
    {
        U64 non_pawns = enemy & ~b.bit_boards[enemy_pawn];
        U64 tmp       = non_pawns & attacks.pawn;
        while (tmp) {
            Square sq  = pop_lsb(tmp);
            PieceType pt = get_type(b.mailbox[sq]);
            add_score(mg, eg, THREAT_BY_PAWN_MG[pt], THREAT_BY_PAWN_EG[pt], c);
        }
    }

    // 2. Minor threats: our N/B attacking enemy rooks or queens
    {
        U64 minor_targets = enemy & (b.bit_boards[them == WHITE ? WR : BR] |
                                     b.bit_boards[them == WHITE ? WQ : BQ]);
        U64 minor_attacks_bb = attacks.by_type[KNIGHT] | attacks.by_type[BISHOP];
        U64 tmp = minor_targets & minor_attacks_bb;
        while (tmp) {
            Square sq  = pop_lsb(tmp);
            PieceType pt = get_type(b.mailbox[sq]);
            add_score(mg, eg, THREAT_BY_MINOR_MG[pt], THREAT_BY_MINOR_EG[pt], c);
        }
    }

    // 3. Rook threats: our rooks attacking enemy queens
    {
        U64 enemy_queens = b.bit_boards[them == WHITE ? WQ : BQ];
        U64 tmp = enemy_queens & attacks.by_type[ROOK];
        if (tmp) {
            int n = popcount(tmp);
            add_score(mg, eg, n * THREAT_BY_ROOK_MG, n * THREAT_BY_ROOK_EG, c);
        }
    }

    // 4. Hanging pieces: our pieces attacked by the enemy with no defence.
    //    Value-weighted so a hanging queen hurts more than a hanging pawn.
    {
        Piece own_king = (c == WHITE) ? WK : BK;
        U64 hanging    = b.occupancies[c] & enemy_attacks.all & ~attacks.all;
        U64 tmp        = hanging;
        while (tmp) {
            Square sq = pop_lsb(tmp);
            Piece p   = b.mailbox[sq];
            if (p == own_king) continue;
            int val    = PTYPE_VALUE[get_type(p)];
            int mg_pen = HANGING_BASE_PENALTY_MG + val / HANGING_VALUE_DIVISOR;
            int eg_pen = HANGING_BASE_PENALTY_EG + val / HANGING_VALUE_DIVISOR;
            // Negative delta for colour c (we are penalising ourselves)
            add_score(mg, eg, -mg_pen, -eg_pen, c);
        }
    }
}

// ============================================================
// Outpost squares
// ============================================================
static void evaluate_outposts(const Board &b, Colour c,
                               const AttackInfo &attacks,
                               const AttackInfo &enemy_attacks,
                               int &mg, int &eg) {
    U64 enemy_pawn_attacks   = enemy_attacks.pawn;
    U64 friendly_pawn_attacks = attacks.pawn;

    U64 knights = (c == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    U64 bishops = (c == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    U64 rooks   = (c == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    U64 queens  = (c == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];
    U64 pawns   = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
    U64 all_p   = b.bit_boards[WP] | b.bit_boards[BP];

    U64 temp = knights;
    while (temp) {
        Square sq = pop_lsb(temp);
        bool on_enemy_half = (c == WHITE) ? get_rank(sq) >= 4 : get_rank(sq) <= 3;
        bool safe      = !(enemy_pawn_attacks    & bb_square(sq));
        bool supported =   friendly_pawn_attacks & bb_square(sq);
        if (on_enemy_half && safe && supported)
            add_score(mg, eg, KNIGHT_OUTPOST_MG, KNIGHT_OUTPOST_EG, c);
    }

    temp = bishops;
    while (temp) {
        Square sq = pop_lsb(temp);
        bool on_enemy_half = (c == WHITE) ? get_rank(sq) >= 4 : get_rank(sq) <= 3;
        bool safe      = !(enemy_pawn_attacks    & bb_square(sq));
        bool supported =   friendly_pawn_attacks & bb_square(sq);
        if (on_enemy_half && safe && supported)
            add_score(mg, eg, BISHOP_OUTPOST_MG, BISHOP_OUTPOST_EG, c);
    }

    temp = rooks;
    while (temp) {
        Square sq       = pop_lsb(temp);
        bool on_seventh = (c == WHITE) ? get_rank(sq) == RANK_7 : get_rank(sq) == RANK_2;
        int mult = openness_multiplier_for_file(pawns, all_p, get_file(sq));
        if (on_seventh || mult > 100)
            add_score(mg, eg, ROOK_OUTPOST_MG, ROOK_OUTPOST_EG, c);
    }

    temp = queens;
    while (temp) {
        Square sq  = pop_lsb(temp);
        bool on_center = (bb_square(sq) & CENTER_MASK) != 0;
        bool safe      = !(enemy_pawn_attacks & bb_square(sq));
        if (on_center && safe)
            add_score(mg, eg, QUEEN_OUTPOST_MG, QUEEN_OUTPOST_EG, c);
    }
}

// ============================================================
// Development / initiative
// ============================================================
static int development_score(const Board &b, Colour c, int phase) {
    if (phase <= MAX_PHASE / 2) return 0;

    int developed = 0;
    if (c == WHITE) {
        if (!(b.bit_boards[WN] & bb_square(make_square(FILE_B, RANK_1)))) developed++;
        if (!(b.bit_boards[WN] & bb_square(make_square(FILE_G, RANK_1)))) developed++;
        if (!(b.bit_boards[WB] & bb_square(make_square(FILE_C, RANK_1)))) developed++;
        if (!(b.bit_boards[WB] & bb_square(make_square(FILE_F, RANK_1)))) developed++;
    } else {
        if (!(b.bit_boards[BN] & bb_square(make_square(FILE_B, RANK_8)))) developed++;
        if (!(b.bit_boards[BN] & bb_square(make_square(FILE_G, RANK_8)))) developed++;
        if (!(b.bit_boards[BB] & bb_square(make_square(FILE_C, RANK_8)))) developed++;
        if (!(b.bit_boards[BB] & bb_square(make_square(FILE_F, RANK_8)))) developed++;
    }

    Square ksq   = king_square(b, c);
    bool castled = (c == WHITE)
        ? (ksq == make_square(FILE_G, RANK_1) || ksq == make_square(FILE_C, RANK_1))
        : (ksq == make_square(FILE_G, RANK_8) || ksq == make_square(FILE_C, RANK_8));

    int raw = developed * DEVELOPMENT_BONUS + (castled ? CASTLED_BONUS : 0);
    return raw * (phase - MAX_PHASE/2) / (MAX_PHASE/2);
}

} // namespace

// ============================================================
// Public entry point
// ============================================================
int evaluate(const Board &b) {
    int mg = 0, eg = 0;
    int phase = phase_score(b);

    AttackInfo white_attacks = build_attack_info(b, WHITE);
    AttackInfo black_attacks = build_attack_info(b, BLACK);

    // ---- Material + PST ----
    // Combined into one loop; bishop pair detected here too.
    int wb = 0, bb_cnt = 0;
    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bitboard = b.bit_boards[p];
        if (!bitboard) continue;
        Colour    c   = get_colour(Piece(p));
        PieceType pt  = get_type(Piece(p));
        int       sign = (c == WHITE) ? 1 : -1;
        const int* mg_pst = PST_MG_TABLE[pt];
        const int* eg_pst = PST_EG_TABLE[pt];
        int        mg_val = PIECE_VALUES_MG[p];
        int        eg_val = PIECE_VALUES_EG[p];
        while (bitboard) {
            int sq     = __builtin_ctzll(bitboard);
            bitboard  &= bitboard - 1;
            int pst_sq = (c == WHITE) ? sq : mirror(sq);
            mg += sign * (mg_val + mg_pst[pst_sq]);
            eg += sign * (eg_val + eg_pst[pst_sq]);
        }
        if (pt == BISHOP) {
            if (c == WHITE) wb++;
            else            bb_cnt++;
        }
    }
    if (wb     >= 2) { mg += BISHOP_PAIR_BONUS_MG; eg += BISHOP_PAIR_BONUS_EG; }
    if (bb_cnt >= 2) { mg -= BISHOP_PAIR_BONUS_MG; eg -= BISHOP_PAIR_BONUS_EG; }

    // ---- Pawn structure ----
    evaluate_pawns(b, WHITE, black_attacks, mg, eg);
    evaluate_pawns(b, BLACK, white_attacks, mg, eg);

    // ---- King safety ----
    evaluate_king_safety(b, WHITE, black_attacks, phase, mg, eg);
    evaluate_king_safety(b, BLACK, white_attacks, phase, mg, eg);

    // ---- Piece activity (mobility + territory) ----
    evaluate_piece_activity(b, WHITE, black_attacks, mg, eg);
    evaluate_piece_activity(b, BLACK, white_attacks, mg, eg);

    // ---- Coordination / connectivity ----
    evaluate_coordination(b, WHITE, white_attacks, mg, eg);
    evaluate_coordination(b, BLACK, black_attacks, mg, eg);

    // ---- Tactical pressure ----
    evaluate_tactical_pressure(b, WHITE, white_attacks, black_attacks, mg, eg);
    evaluate_tactical_pressure(b, BLACK, black_attacks, white_attacks, mg, eg);

    // ---- Threats ----
    evaluate_threats(b, WHITE, white_attacks, black_attacks, mg, eg);
    evaluate_threats(b, BLACK, black_attacks, white_attacks, mg, eg);

    // ---- Outposts ----
    evaluate_outposts(b, WHITE, white_attacks, black_attacks, mg, eg);
    evaluate_outposts(b, BLACK, black_attacks, white_attacks, mg, eg);

    // ---- Development / initiative ----
    int dev_diff     = development_score(b, WHITE, phase) - development_score(b, BLACK, phase);
    int opening_scale = phase;
    mg += dev_diff * opening_scale / MAX_PHASE;

    // ---- Tempo ----
    if (b.side_to_move == WHITE) { mg += TEMPO_BONUS_MG; eg += TEMPO_BONUS_EG; }
    else                          { mg -= TEMPO_BONUS_MG; eg -= TEMPO_BONUS_EG; }

    // ---- Tapered interpolation ----
    int score = (mg * phase + eg * (MAX_PHASE - phase)) / MAX_PHASE;
    return b.side_to_move == WHITE ? score : -score;
}
} // namespace SHAYVERI
