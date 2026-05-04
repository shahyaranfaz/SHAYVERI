// texel_eval.cpp — SHAYVERI texel-tuner evaluation class implementation.
//
// This file mirrors evaluate.cpp but accumulates per-feature integer counts
// (a "trace") rather than a scalar score.  The counts become the coefficients
// fed to the GediminasMasaitis texel-tuner.
//
// Non-linear parts of the evaluation (king safety, development score,
// piece-value-weighted hanging / undefended penalties) cannot be expressed as
// a simple coefficient × parameter product.  They are captured in the
// EvalResult::score "additional_score" field:
//
//   additional_score = full_engine_eval(white perspective)
//                    − Σ (initial_param[i] ⋅ coefficient[i])  (tapered)
//
// At initialization the predicted score exactly equals the engine eval; the
// tuner then adjusts the linear parameters to minimise MSE while the
// additional_score residual is held fixed per position.
//
// When building inside the texel-tuner source tree, replace the TAPERED and
// chess.hpp includes as noted in texel_eval.h.

#include "tune.h"

#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "types.h"

// When compiled as part of the texel-tuner, chess.hpp is available on the
// include path.  The guard prevents compilation errors inside the SHAYVERI
// standalone build where chess.hpp is absent.
#ifdef CHESS_HPP_INCLUDED
    #include "chess.hpp"
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace SHAYVERI;
using namespace SHAYVERI::Tune;

// Convenience
static inline int pcnt(U64 bb)  { return __builtin_popcountll(bb); }
static inline int mirr(int sq)  { return (7 - sq / 8) * 8 + (sq % 8); }

// (0 = pure endgame, 24 = pure opening/middlegame)
static int phase_of(const Board &b) {
    int p = pcnt(b.bit_boards[WN] | b.bit_boards[BN])
          + pcnt(b.bit_boards[WB] | b.bit_boards[BB])
          + pcnt(b.bit_boards[WR] | b.bit_boards[BR]) * 2
          + pcnt(b.bit_boards[WQ] | b.bit_boards[BQ]) * 4;
    return std::min(p, MAX_PHASE);
}

// Pre-baked masks (copies of evaluate.cpp static data)
static const std::array<U64, 8> FILE_MASKS = [] {
    std::array<U64, 8> m{};
    for (int f = 0; f < 8; ++f)
        for (int r = 0; r < 8; ++r) m[f] |= 1ULL << (r * 8 + f);
    return m;
}();

static const std::array<U64, 8> RANK_MASKS = [] {
    std::array<U64, 8> m{};
    for (int r = 0; r < 8; ++r)
        for (int f = 0; f < 8; ++f) m[r] |= 1ULL << (r * 8 + f);
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

static const std::array<std::array<U64, 8>, 2> FWD_RANK = [] {
    std::array<std::array<U64, 8>, 2> m{};
    for (int r = 0; r < 8; ++r) {
        for (int rr = r + 1; rr < 8; ++rr) m[WHITE][r] |= RANK_MASKS[rr];
        for (int rr = 0;     rr < r;  ++rr) m[BLACK][r] |= RANK_MASKS[rr];
    }
    return m;
}();

static const U64 CENTER_SQ =
    (1ULL << (3*8+3)) | (1ULL << (3*8+4)) |
    (1ULL << (4*8+3)) | (1ULL << (4*8+4));

static const U64 EXT_CENTER_SQ =
    (1ULL<<(2*8+2))|(1ULL<<(2*8+3))|(1ULL<<(2*8+4))|(1ULL<<(2*8+5))|
    (1ULL<<(3*8+2))|(1ULL<<(3*8+5))|
    (1ULL<<(4*8+2))|(1ULL<<(4*8+5))|
    (1ULL<<(5*8+2))|(1ULL<<(5*8+3))|(1ULL<<(5*8+4))|(1ULL<<(5*8+5));

// Attack-info helper (mirrors evaluate.cpp)
struct AInfo {
    U64 by_type[7]{};
    U64 all = 0, pawn = 0, non_pawn = 0;
    std::array<int, 64> cnt{}, np_cnt{};
};

static void add_atk(AInfo& a, U64 bb, PieceType pt) {
    a.all |= bb; a.by_type[pt] |= bb;
    if (pt == PAWN) { a.pawn |= bb; } else { a.non_pawn |= bb; }
    U64 t = bb;
    while (t) { Square s = pop_lsb(t); a.cnt[s]++; if (pt != PAWN) a.np_cnt[s]++; }
}

static AInfo build_ai(const Board& b, Colour c) {
    AInfo a;
    U64 t;
    t = (c==WHITE)?b.bit_boards[WP]:b.bit_boards[BP];
    while(t){Square s=pop_lsb(t);add_atk(a,pawn_attacks(c,s),PAWN);}
    t = (c==WHITE)?b.bit_boards[WN]:b.bit_boards[BN];
    while(t){Square s=pop_lsb(t);add_atk(a,knight_attacks(s),KNIGHT);}
    t = (c==WHITE)?b.bit_boards[WB]:b.bit_boards[BB];
    while(t){Square s=pop_lsb(t);add_atk(a,bishop_attacks(s,b.occupied),BISHOP);}
    t = (c==WHITE)?b.bit_boards[WR]:b.bit_boards[BR];
    while(t){Square s=pop_lsb(t);add_atk(a,rook_attacks(s,b.occupied),ROOK);}
    t = (c==WHITE)?b.bit_boards[WQ]:b.bit_boards[BQ];
    while(t){Square s=pop_lsb(t);add_atk(a,queen_attacks(s,b.occupied),QUEEN);}
    add_atk(a, king_attacks(king_square(b,c)), KING);
    return a;
}

// Pawn-structure predicates (mirrors evaluate.cpp)
static bool is_passed(Colour c, Square sq, U64 enemy_pawns) {
    int f = get_file(sq), r = get_rank(sq);
    U64 fwd  = FWD_RANK[c][r];
    U64 fmsk = FILE_MASKS[f] | ADJ_FILE_MASKS[f];
    return (enemy_pawns & fwd & fmsk) == 0;
}

static bool is_supported(Colour c, Square sq, U64 own_pawns) {
    int f = get_file(sq), r = get_rank(sq);
    if (c == WHITE) {
        if (r == 0) return false;
        if (f > 0 && (own_pawns & bb_square(make_square(File(f-1), Rank(r-1))))) return true;
        if (f < 7 && (own_pawns & bb_square(make_square(File(f+1), Rank(r-1))))) return true;
    } else {
        if (r == 7) return false;
        if (f > 0 && (own_pawns & bb_square(make_square(File(f-1), Rank(r+1))))) return true;
        if (f < 7 && (own_pawns & bb_square(make_square(File(f+1), Rank(r+1))))) return true;
    }
    return false;
}

static bool is_backward(Colour c, Square sq, U64 own_pawns, U64 enemy_pawn_atk, U64 occ) {
    int f = get_file(sq), r = get_rank(sq);
    U64 adj = ADJ_FILE_MASKS[f];
    U64 sof = 0;
    if (c == WHITE) { for (int rr = r; rr < 8; ++rr) sof |= RANK_MASKS[rr]; }
    else            { for (int rr = 0; rr <= r; ++rr) sof |= RANK_MASKS[rr]; }
    bool has_support = (own_pawns & adj & sof) != 0;
    Square fwd = SQ_NONE;
    if (c == WHITE && r < 7) fwd = sq + 8;
    if (c == BLACK && r > 0) fwd = sq - 8;
    bool blocked    = (fwd != SQ_NONE) && (occ              & bb_square(fwd));
    bool controlled = (fwd != SQ_NONE) && (enemy_pawn_atk   & bb_square(fwd));
    return !has_support && (blocked || controlled);
}

static bool has_clear_wing(int file, U64 enemy_pawns) {
    if (file <= FILE_B) {
        U64 w = FILE_MASKS[0]|FILE_MASKS[1]|FILE_MASKS[2];
        return (enemy_pawns & w) == 0;
    }
    if (file >= FILE_G) {
        U64 w = FILE_MASKS[5]|FILE_MASKS[6]|FILE_MASKS[7];
        return (enemy_pawns & w) == 0;
    }
    return false;
}

// Openness helpers (used for additional_score computation)
static int file_openness(U64 own_p, U64 all_p, int file) {
    U64 m = FILE_MASKS[file];
    bool has_own = (own_p & m) != 0;
    bool has_any = (all_p & m) != 0;
    if (!has_any)  return OPEN_FILE_MULTIPLIER;
    if (!has_own)  return SEMI_OPEN_FILE_MULTIPLIER;
    return CLOSED_FILE_MULTIPLIER;
}

// Pin counter (mirrors evaluate.cpp)
static int count_pins(const Board& b, Colour attacker) {
    Colour def  = flip(attacker);
    Square king = king_square(b, def);
    int kf = get_file(king), kr = get_rank(king);
    int count = 0;

    auto chk = [&](Square from, bool ortho, bool diag) {
        int f = get_file(from), r = get_rank(from);
        int df = kf-f, dr = kr-r, sf=0, sr=0;
        if      (df==0&&dr!=0)              { if(!ortho) return; sr=(dr>0)?1:-1; }
        else if (dr==0&&df!=0)              { if(!ortho) return; sf=(df>0)?1:-1; }
        else if (std::abs(df)==std::abs(dr)){ if(!diag)  return; sf=(df>0)?1:-1; sr=(dr>0)?1:-1; }
        else return;
        int cf=f+sf, cr=r+sr;
        Square pinned = SQ_NONE;
        Piece king_p  = (def==WHITE)?WK:BK;
        while ((cf!=kf||cr!=kr) && cf>=0&&cf<8&&cr>=0&&cr<8) {
            Square sq = make_square(File(cf),Rank(cr));
            Piece  p  = b.mailbox[sq];
            if (p != NONE_PIECE) {
                if (pinned == SQ_NONE) {
                    if (get_colour(p)==def && p!=king_p) pinned=sq;
                    else return;
                } else {
                    if (p==king_p) count++;
                    return;
                }
            }
            cf+=sf; cr+=sr;
        }
        if (pinned != SQ_NONE) count++;
    };

    U64 rooks   = (attacker==WHITE)?b.bit_boards[WR]:b.bit_boards[BR];
    U64 bishops = (attacker==WHITE)?b.bit_boards[WB]:b.bit_boards[BB];
    U64 queens  = (attacker==WHITE)?b.bit_boards[WQ]:b.bit_boards[BQ];
    U64 tmp;
    tmp=rooks;   while(tmp){Square s=pop_lsb(tmp);chk(s,true,false);}
    tmp=bishops; while(tmp){Square s=pop_lsb(tmp);chk(s,false,true);}
    tmp=queens;  while(tmp){Square s=pop_lsb(tmp);chk(s,true,true);}
    return count;
}

// Overloaded-defender counter (mirrors evaluate.cpp)
static int count_overloaded(const Board& b, Colour defender, const AInfo& atk_info) {
    U64 pieces = b.occupancies[defender];
    int cnt = 0;
    while (pieces) {
        Square sq  = pop_lsb(pieces);
        Piece  p   = b.mailbox[sq];
        U64 def_bb = 0;
        switch (get_type(p)) {
            case PAWN:   def_bb = pawn_attacks(defender, sq); break;
            case KNIGHT: def_bb = knight_attacks(sq); break;
            case BISHOP: def_bb = bishop_attacks(sq, b.occupied); break;
            case ROOK:   def_bb = rook_attacks(sq, b.occupied); break;
            case QUEEN:  def_bb = queen_attacks(sq, b.occupied); break;
            case KING:   def_bb = king_attacks(sq); break;
            default: break;
        }
        def_bb &= b.occupancies[defender];
        int n = 0;
        U64 tmp = def_bb;
        while (tmp) { Square dsq = pop_lsb(tmp); if (atk_info.cnt[dsq] > 0) n++; }
        if (n >= 2) cnt++;
    }
    return cnt;
}

// Trace struct
// Every field is I32[2] with [0]=WHITE count, [1]=BLACK count.
struct Trace {
    I32 material[5][2];        // piece types PAWN..QUEEN (0-indexed)
    I32 pst[6][64][2];         // piece types PAWN..KING × square
    I32 bishop_pair[2];
    // Pawn structure
    I32 isolated[2];
    I32 doubled[2];
    I32 backward[2];
    I32 supported[2];
    I32 weak_pawn[2];
    I32 island[2];
    I32 pawn_center[2];
    I32 pawn_ext_center[2];
    I32 storm_base[2];         // count of storm pawns near enemy king
    I32 storm_rank[2];         // sum of rank advancements for storm pawns
    // Passed pawns (rank indices 1-6 used; 0 and 7 always zero)
    I32 passed_rank[8][2];
    I32 candidate[2];
    I32 connected_passed[2];
    I32 outside_passed[2];
    // Mobility (raw square count; openness factor is absorbed into additional_score)
    I32 mob_knight[2];
    I32 mob_bishop[2];
    I32 mob_rook[2];
    I32 mob_queen[2];
    // Territory
    I32 center_bonus[2];
    I32 ext_center_bonus[2];
    I32 enemy_half[2];
    I32 seventh_rook[2];
    // Coordination
    I32 defended[2];
    I32 shared_target[2];
    I32 battery_rq[2];
    I32 battery_bq[2];
    I32 support_chain[2];
    // Tactical pressure (can be negative for unrec_pressure)
    I32 pin[2];
    I32 overloaded[2];
    I32 unrec_pressure[2];
    // Threats
    I32 threat_pawn_knight[2];
    I32 threat_pawn_bishop[2];
    I32 threat_pawn_rook[2];
    I32 threat_pawn_queen[2];
    I32 threat_minor_rook[2];
    I32 threat_minor_queen[2];
    I32 threat_rook[2];
    // Outposts
    I32 knight_outpost[2];
    I32 bishop_outpost[2];
    I32 rook_outpost[2];
    I32 queen_outpost[2];
    // Tempo (+1 for the side to move)
    I32 tempo[2];
};

// Trace evaluation
// Returns a Trace whose fields record how many times each linear parameter
// contributes, from White's (index 0) and Black's (index 1) perspective.
static Trace trace_evaluate(const Board& b) {
    Trace tr{};

    AInfo wa = build_ai(b, WHITE);
    AInfo ba = build_ai(b, BLACK);

    // Material + PST

    int wb = 0, bb_cnt = 0;  // bishop counts
    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = b.bit_boards[p];
        if (!bb) continue;
        Colour    c  = get_colour(Piece(p));
        PieceType pt = get_type(Piece(p));
        int       ci = (c == WHITE) ? 0 : 1;
        int       pi = static_cast<int>(pt) - 1;  // 0=PAWN..5=KING

        if (pt != KING) tr.material[pi][ci] += pcnt(bb);  // PAWN-QUEEN material

        while (bb) {
            int sq     = __builtin_ctzll(bb); bb &= bb - 1;
            int pst_sq = (c == WHITE) ? sq : mirr(sq);
            tr.pst[pi][pst_sq][ci]++;
        }
        if (pt == BISHOP) { if (c == WHITE) wb++; else bb_cnt++; }
    }
    if (wb     >= 2) tr.bishop_pair[0]++;
    if (bb_cnt >= 2) tr.bishop_pair[1]++;

    // Pawn structure
    for (int ci = 0; ci < 2; ++ci) {
        Colour c = (ci == 0) ? WHITE : BLACK;
        const AInfo& ea = (c == WHITE) ? ba : wa;

        U64 pawns       = (c == WHITE) ? b.bit_boards[WP] : b.bit_boards[BP];
        U64 enemy_pawns = (c == WHITE) ? b.bit_boards[BP] : b.bit_boards[WP];
        U64 passed_bb   = 0;
        std::array<int, 8> file_cnt{};
        std::array<bool, 64> is_pass{};

        U64 tmp = pawns;
        while (tmp) {
            Square sq = pop_lsb(tmp);
            file_cnt[get_file(sq)]++;
            if (is_passed(c, sq, enemy_pawns)) { is_pass[sq] = true; passed_bb |= bb_square(sq); }
        }

        // Islands
        int islands = 0; bool in_isl = false;
        for (int f = 0; f < 8; ++f) {
            if (file_cnt[f] > 0) { if (!in_isl) { islands++; in_isl = true; } }
            else { in_isl = false; }
        }
        if (islands > 1) tr.island[ci] += islands - 1;

        // Doubled
        for (int f = 0; f < 8; ++f)
            if (file_cnt[f] > 1) tr.doubled[ci] += file_cnt[f] - 1;

        // Per-pawn features
        tmp = pawns;
        while (tmp) {
            Square sq      = pop_lsb(tmp);
            int    f       = get_file(sq);
            int    r       = get_rank(sq);
            int    rel     = (c == WHITE) ? r : 7 - r;
            U64    sq_bb   = bb_square(sq);
            bool   iso     = (pawns & ADJ_FILE_MASKS[f]) == 0;
            bool   supp    = is_supported(c, sq, pawns);
            bool   weak    = (ea.pawn & sq_bb) && !supp;

            if (iso)  tr.isolated[ci]++;
            if (supp) tr.supported[ci]++;
            if (weak) tr.weak_pawn[ci]++;

            if (is_pass[sq]) {
                tr.passed_rank[rel][ci]++;
                if (has_clear_wing(f, enemy_pawns)) tr.outside_passed[ci]++;
                if (passed_bb & ADJ_FILE_MASKS[f])  tr.connected_passed[ci]++;
            } else {
                Square fwd = SQ_NONE;
                if (c == WHITE && r < 7) fwd = sq + 8;
                if (c == BLACK && r > 0) fwd = sq - 8;
                bool fwd_empty = (fwd != SQ_NONE) && !(b.occupied & bb_square(fwd));
                if (fwd_empty) {
                    U64 fwd_mask = FWD_RANK[c][r];
                    bool same_clear = (enemy_pawns & FILE_MASKS[f] & fwd_mask) == 0;
                    int  adj_en     = pcnt(enemy_pawns & ADJ_FILE_MASKS[f] & fwd_mask);
                    if (same_clear && adj_en <= 1) tr.candidate[ci]++;
                }
                if (is_backward(c, sq, pawns, ea.pawn, b.occupied)) tr.backward[ci]++;
            }

            if      (sq_bb & CENTER_SQ)     tr.pawn_center[ci]++;
            else if (sq_bb & EXT_CENTER_SQ) tr.pawn_ext_center[ci]++;
        }

        // Pawn storm
        Square ek   = king_square(b, flip(c));
        int    ekf  = get_file(ek);
        tmp = pawns;
        while (tmp) {
            Square sq = pop_lsb(tmp);
            int sf = get_file(sq), sr = get_rank(sq);
            if (std::abs(sf - ekf) <= 1) {
                if ((c == WHITE && sr >= 3) || (c == BLACK && sr <= 4)) {
                    int adv = (c == WHITE) ? sr : 7 - sr;
                    tr.storm_base[ci]++;
                    tr.storm_rank[ci] += adv;
                }
            }
        }
    }

    // Piece activity (mobility + territory)
    for (int ci = 0; ci < 2; ++ci) {
        Colour c   = (ci == 0) ? WHITE : BLACK;
        const AInfo& ea = (c == WHITE) ? ba : wa;
        U64 friendly = b.occupancies[c];
        U64 safe     = ~friendly & ~ea.pawn;

        // Knights
        U64 tmp = (c==WHITE)?b.bit_boards[WN]:b.bit_boards[BN];
        while (tmp) {
            Square sq   = pop_lsb(tmp);
            U64 sq_bb   = bb_square(sq);
            U64 atk     = knight_attacks(sq) & safe;
            tr.mob_knight[ci] += pcnt(atk);
            if      (sq_bb & CENTER_SQ)     tr.center_bonus[ci]++;
            else if (sq_bb & EXT_CENTER_SQ) tr.ext_center_bonus[ci]++;
            if ((c==WHITE && get_rank(sq)>=4)||(c==BLACK && get_rank(sq)<=3)) tr.enemy_half[ci]++;
        }

        // Bishops
        tmp = (c==WHITE)?b.bit_boards[WB]:b.bit_boards[BB];
        while (tmp) {
            Square sq   = pop_lsb(tmp);
            U64 sq_bb   = bb_square(sq);
            U64 atk     = bishop_attacks(sq, b.occupied) & safe;
            tr.mob_bishop[ci] += pcnt(atk);  // openness factor absorbed into additional_score
            if      (sq_bb & CENTER_SQ)     tr.center_bonus[ci]++;
            else if (sq_bb & EXT_CENTER_SQ) tr.ext_center_bonus[ci]++;
            if ((c==WHITE && get_rank(sq)>=4)||(c==BLACK && get_rank(sq)<=3)) tr.enemy_half[ci]++;
        }

        // Rooks
        tmp = (c==WHITE)?b.bit_boards[WR]:b.bit_boards[BR];
        while (tmp) {
            Square sq   = pop_lsb(tmp);
            U64 atk     = rook_attacks(sq, b.occupied) & safe;
            tr.mob_rook[ci] += pcnt(atk);    // openness factor absorbed into additional_score
            if ((c==WHITE && get_rank(sq)>=4)||(c==BLACK && get_rank(sq)<=3)) tr.enemy_half[ci]++;
        }

        // Queens
        tmp = (c==WHITE)?b.bit_boards[WQ]:b.bit_boards[BQ];
        while (tmp) {
            Square sq   = pop_lsb(tmp);
            U64 sq_bb   = bb_square(sq);
            U64 atk     = queen_attacks(sq, b.occupied) & safe;
            tr.mob_queen[ci] += pcnt(atk);   // openness factor absorbed into additional_score
            if      (sq_bb & CENTER_SQ)     tr.center_bonus[ci]++;
            else if (sq_bb & EXT_CENTER_SQ) tr.ext_center_bonus[ci]++;
            if ((c==WHITE && get_rank(sq)>=4)||(c==BLACK && get_rank(sq)<=3)) tr.enemy_half[ci]++;
        }
    }

    // Coordination
    for (int ci = 0; ci < 2; ++ci) {
        Colour c  = (ci == 0) ? WHITE : BLACK;
        const AInfo& atk = (c == WHITE) ? wa : ba;
        U64 pieces = b.occupancies[c];
        U64 enemy  = b.occupancies[flip(c)];

        // Defended pieces
        U64 tmp = pieces;
        while (tmp) { Square sq = pop_lsb(tmp); if (atk.cnt[sq] > 0) tr.defended[ci]++; }

        // Shared attack targets
        tmp = enemy;
        while (tmp) { Square sq = pop_lsb(tmp); if (atk.cnt[sq] >= 2) tr.shared_target[ci]++; }

        // Batteries
        U64 rooks   = (c==WHITE)?b.bit_boards[WR]:b.bit_boards[BR];
        U64 bishops = (c==WHITE)?b.bit_boards[WB]:b.bit_boards[BB];
        U64 queens  = (c==WHITE)?b.bit_boards[WQ]:b.bit_boards[BQ];
        tmp = rooks;   while(tmp){Square s=pop_lsb(tmp);if(rook_attacks(s,b.occupied)&queens) tr.battery_rq[ci]++;}
        tmp = bishops; while(tmp){Square s=pop_lsb(tmp);if(bishop_attacks(s,b.occupied)&queens) tr.battery_bq[ci]++;}

        // Support chains
        tmp = pieces;
        while (tmp) { Square sq = pop_lsb(tmp); if (atk.np_cnt[sq] > 0) tr.support_chain[ci]++; }
    }

    // Tactical pressure
    // Note: undefended attack and hanging are nonlinear (value-scaled) and are
    // excluded from the trace; they appear in additional_score instead.
    for (int ci = 0; ci < 2; ++ci) {
        Colour c  = (ci == 0) ? WHITE : BLACK;
        const AInfo& atk = (c == WHITE) ? wa : ba;
        const AInfo& ea  = (c == WHITE) ? ba : wa;

        int pins = count_pins(b, c);
        tr.pin[ci] += pins;

        int over = count_overloaded(b, flip(c), atk);
        tr.overloaded[ci] += over;

        int atk_on_e = pcnt(atk.all & b.occupancies[flip(c)]);
        int atk_on_u = pcnt(ea.all  & b.occupancies[c]);
        tr.unrec_pressure[ci] += atk_on_e - atk_on_u;
    }

    // Threats
    for (int ci = 0; ci < 2; ++ci) {
        Colour c    = (ci == 0) ? WHITE : BLACK;
        Colour them = flip(c);
        const AInfo& atk = (c == WHITE) ? wa : ba;
        U64 enemy   = b.occupancies[them];
        Piece ep    = (them == WHITE) ? WP : BP;
        Piece er    = (them == WHITE) ? WR : BR;
        Piece eq    = (them == WHITE) ? WQ : BQ;

        // Pawn threats
        U64 non_p = enemy & ~b.bit_boards[ep];
        U64 tmp   = non_p & atk.pawn;
        while (tmp) {
            Square sq  = pop_lsb(tmp);
            PieceType pt = get_type(b.mailbox[sq]);
            switch (pt) {
                case KNIGHT: tr.threat_pawn_knight[ci]++; break;
                case BISHOP: tr.threat_pawn_bishop[ci]++; break;
                case ROOK:   tr.threat_pawn_rook[ci]++;   break;
                case QUEEN:  tr.threat_pawn_queen[ci]++;  break;
                default: break;
            }
        }

        // Minor threats (N/B attacking R or Q)
        U64 minor_tgt = enemy & (b.bit_boards[er] | b.bit_boards[eq]);
        U64 minor_atk = atk.by_type[KNIGHT] | atk.by_type[BISHOP];
        tmp = minor_tgt & minor_atk;
        while (tmp) {
            Square sq  = pop_lsb(tmp);
            PieceType pt = get_type(b.mailbox[sq]);
            if      (pt == ROOK)  tr.threat_minor_rook[ci]++;
            else if (pt == QUEEN) tr.threat_minor_queen[ci]++;
        }

        // Rook attacks queen
        tmp = b.bit_boards[eq] & atk.by_type[ROOK];
        if (tmp) tr.threat_rook[ci] += pcnt(tmp);
    }

    // Outposts
    for (int ci = 0; ci < 2; ++ci) {
        Colour c  = (ci == 0) ? WHITE : BLACK;
        const AInfo& atk = (c == WHITE) ? wa : ba;
        const AInfo& ea  = (c == WHITE) ? ba : wa;
        U64 pawns = (c==WHITE)?b.bit_boards[WP]:b.bit_boards[BP];
        U64 all_p = b.bit_boards[WP]|b.bit_boards[BP];

        U64 tmp = (c==WHITE)?b.bit_boards[WN]:b.bit_boards[BN];
        while (tmp) {
            Square sq = pop_lsb(tmp);
            bool on_eh = (c==WHITE) ? get_rank(sq)>=4 : get_rank(sq)<=3;
            bool safe  = !(ea.pawn & bb_square(sq));
            bool supp  = (atk.pawn & bb_square(sq)) != 0;
            if (on_eh && safe && supp) tr.knight_outpost[ci]++;
        }

        tmp = (c==WHITE)?b.bit_boards[WB]:b.bit_boards[BB];
        while (tmp) {
            Square sq = pop_lsb(tmp);
            bool on_eh = (c==WHITE) ? get_rank(sq)>=4 : get_rank(sq)<=3;
            bool safe  = !(ea.pawn & bb_square(sq));
            bool supp  = (atk.pawn & bb_square(sq)) != 0;
            if (on_eh && safe && supp) tr.bishop_outpost[ci]++;
        }

        tmp = (c==WHITE)?b.bit_boards[WR]:b.bit_boards[BR];
        while (tmp) {
            Square sq  = pop_lsb(tmp);
            bool on7   = (c==WHITE) ? get_rank(sq)==RANK_7 : get_rank(sq)==RANK_2;
            int  mult  = file_openness(pawns, all_p, get_file(sq));
            if (on7 || mult > 100) tr.rook_outpost[ci]++;
        }

        tmp = (c==WHITE)?b.bit_boards[WQ]:b.bit_boards[BQ];
        while (tmp) {
            Square sq = pop_lsb(tmp);
            bool on_c = (bb_square(sq) & CENTER_SQ) != 0;
            bool safe = !(ea.pawn & bb_square(sq));
            if (on_c && safe) tr.queen_outpost[ci]++;
        }
    }

    // Tempo
    tr.tempo[(b.side_to_move == WHITE) ? 0 : 1]++;

    return tr;
}

// Convert trace to coefficient vector
// The ordering here MUST exactly match get_initial_parameters() and
// print_parameters().
static coefficients_t get_coefficients(const Trace& tr) {
    coefficients_t c;

    // 1. Material (5 types: PAWN-QUEEN)
    push_coeff_arr(c, tr.material + 1, 4);

    // 2. PST (6 types × 64 squares)
    for (int pt = 0; pt < 6; ++pt)
        push_coeff_arr(c, tr.pst[pt], 64);

    // 3. Bishop pair
    push_coeff(c, tr.bishop_pair);

    // 4. Pawn structure
    push_coeff(c, tr.isolated);
    push_coeff(c, tr.doubled);
    push_coeff(c, tr.backward);
    push_coeff(c, tr.supported);
    push_coeff(c, tr.weak_pawn);
    push_coeff(c, tr.island);
    push_coeff(c, tr.pawn_center);
    push_coeff(c, tr.pawn_ext_center);

    // 5. Pawn storm
    push_coeff(c, tr.storm_base);
    push_coeff(c, tr.storm_rank);

    // 6. Passed pawns (ranks 1-6)
    push_coeff_arr(c, tr.passed_rank + 1, 6);

    // 7. Candidate / connected / outside
    push_coeff(c, tr.candidate);
    push_coeff(c, tr.connected_passed);
    push_coeff(c, tr.outside_passed);

    // 8. Mobility
    push_coeff(c, tr.mob_knight);
    push_coeff(c, tr.mob_bishop);
    push_coeff(c, tr.mob_rook);
    push_coeff(c, tr.mob_queen);

    // 9. Territory
    push_coeff(c, tr.center_bonus);
    push_coeff(c, tr.ext_center_bonus);
    push_coeff(c, tr.enemy_half);
    push_coeff(c, tr.seventh_rook);

    // 10. Coordination
    push_coeff(c, tr.defended);
    push_coeff(c, tr.shared_target);
    push_coeff(c, tr.battery_rq);
    push_coeff(c, tr.battery_bq);
    push_coeff(c, tr.support_chain);

    // 11. Tactical
    push_coeff(c, tr.pin);
    push_coeff(c, tr.overloaded);
    push_coeff(c, tr.unrec_pressure);

    // 12. Threats
    push_coeff(c, tr.threat_pawn_knight);
    push_coeff(c, tr.threat_pawn_bishop);
    push_coeff(c, tr.threat_pawn_rook);
    push_coeff(c, tr.threat_pawn_queen);
    push_coeff(c, tr.threat_minor_rook);
    push_coeff(c, tr.threat_minor_queen);
    push_coeff(c, tr.threat_rook);

    // 13. Outposts
    push_coeff(c, tr.knight_outpost);
    push_coeff(c, tr.bishop_outpost);
    push_coeff(c, tr.rook_outpost);
    push_coeff(c, tr.queen_outpost);

    // 14. Tempo
    push_coeff(c, tr.tempo);

    return c;
}

// PST tables (pointers mirroring evaluate.cpp)
static const int* const MG_PTAB[7] = {nullptr, PST_PAWN_MG, PST_KNIGHT_MG,
                                       PST_BISHOP_MG, PST_ROOK_MG, PST_QUEEN_MG, PST_KING_MG};
static const int* const EG_PTAB[7] = {nullptr, PST_PAWN_EG, PST_KNIGHT_EG,
                                       PST_BISHOP_EG, PST_ROOK_EG, PST_QUEEN_EG, PST_KING_EG};

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// TexelTuner public methods
// ─────────────────────────────────────────────────────────────────────────────

// Parameter ordering (438 pairs total):
//   0-4    : piece values (PAWN-QUEEN)
//   5-388  : PST (6 types × 64 sq)
//   389    : bishop pair
//   390-397: pawn structure (isolated, doubled, backward, supported, weak, island, center, ext_center)
//   398-399: pawn storm (base, rank_mult)
//   400-405: passed ranks 1-6
//   406-408: candidate, connected_passed, outside_passed
//   409-412: mobility (knight, bishop, rook, queen)
//   413-416: territory (center, ext_center, enemy_half, seventh_rook)
//   418-422: coordination (defended, shared_target, battery_rq, battery_bq, support_chain)
//   423-425: tactical (pin, overloaded, unrec_pressure)
//   426-432: threats (pawn×4, minor×2, rook)
//   433-436: outposts (knight, bishop, rook, queen)
//   437    : tempo

parameters_t TexelTuner::get_initial_parameters() {
    parameters_t p;
    using namespace SHAYVERI::Tune;

    // 1. Material (piece values MG/EG, PAWN-QUEEN)
    // Piece enum: WP=1, WN=2, WB=3, WR=4, WQ=5
    for (int pt = 2; pt <= 5; ++pt)
        push_pair(p, PIECE_VALUES_MG[pt], PIECE_VALUES_EG[pt]);

    // 2. PST (6 types × 64 squares)
    for (int pt = 1; pt <= 6; ++pt) {
        for (int sq = 0; sq < 64; ++sq)
            push_pair(p, MG_PTAB[pt][sq], EG_PTAB[pt][sq]);
    }

    // 3. Bishop pair {MG, MG/2}
    push_pair(p, BISHOP_PAIR_BONUS, BISHOP_PAIR_BONUS / 2);

    // 4. Pawn structure
    push_pair(p, ISOLATED_PAWN_PENALTY_MG,  ISOLATED_PAWN_PENALTY_EG);
    push_pair(p, DOUBLED_PAWN_PENALTY_MG,   DOUBLED_PAWN_PENALTY_EG);
    push_pair(p, BACKWARD_PAWN_PENALTY_MG,  BACKWARD_PAWN_PENALTY_EG);
    push_pair(p, SUPPORTED_PAWN_BONUS_MG,   0);
    push_pair(p, WEAK_PAWN_PENALTY_MG,      0);
    push_pair(p, PAWN_ISLAND_PENALTY_MG,    PAWN_ISLAND_PENALTY_EG);
    push_pair(p, PAWN_CENTER_BONUS_MG,      PAWN_CENTER_BONUS_EG);
    push_pair(p, PAWN_EXT_CENTER_BONUS_MG,  PAWN_EXT_CENTER_BONUS_EG);

    // 5. Pawn storm {storm_base MG, storm_rank MG}
    push_pair(p, PAWN_STORM_BASE,      0);
    push_pair(p, PAWN_STORM_RANK_MULT, 0);

    // 6. Passed pawns ranks 1-6
    for (int r = 1; r <= 6; ++r)
        push_pair(p, PASSED_PAWN_BONUS_MG[r], PASSED_PAWN_BONUS_EG[r]);

    // 7. Candidate / connected / outside
    push_pair(p, CANDIDATE_PAWN_BONUS_MG,   CANDIDATE_PAWN_BONUS_EG);
    push_pair(p, CONNECTED_PASSED_BONUS_MG, CONNECTED_PASSED_BONUS_EG);
    push_pair(p, OUTSIDE_PASSED_BONUS_MG,   OUTSIDE_PASSED_BONUS_EG);

    // 8. Mobility
    push_pair(p, MOBILITY_KNIGHT_MG, MOBILITY_KNIGHT_EG);
    push_pair(p, MOBILITY_BISHOP_MG, MOBILITY_BISHOP_EG);
    push_pair(p, MOBILITY_ROOK_MG,   MOBILITY_ROOK_EG);
    push_pair(p, MOBILITY_QUEEN_MG,  MOBILITY_QUEEN_EG);

    // 9. Territory
    push_pair(p, CENTER_BONUS,          CENTER_BONUS / 2);
    push_pair(p, EXT_CENTER_BONUS,      EXT_CENTER_BONUS / 2);
    push_pair(p, ENEMY_HALF_BONUS,      ENEMY_HALF_BONUS);
    push_pair(p, SEVENTH_RANK_BONUS_MG, SEVENTH_RANK_BONUS_EG);

    // 10. Coordination
    push_pair(p, DEFENDED_PIECE_BONUS,       DEFENDED_PIECE_BONUS);
    push_pair(p, SHARED_TARGET_BONUS,        SHARED_TARGET_BONUS);
    push_pair(p, BATTERY_ROOK_QUEEN_BONUS,   BATTERY_ROOK_QUEEN_BONUS);
    push_pair(p, BATTERY_BISHOP_QUEEN_BONUS, BATTERY_BISHOP_QUEEN_BONUS);
    push_pair(p, SUPPORT_CHAIN_BONUS,        SUPPORT_CHAIN_BONUS);

    // 11. Tactical
    push_pair(p, PIN_BONUS,                       PIN_BONUS / 2);
    push_pair(p, OVERLOADED_DEFENDER_BONUS,        OVERLOADED_DEFENDER_BONUS / 2);
    push_pair(p, UNRECIPROCATED_PRESSURE_BONUS,    UNRECIPROCATED_PRESSURE_BONUS / 2);

    // 12. Threats
    push_pair(p, THREAT_BY_PAWN_MG[KNIGHT], THREAT_BY_PAWN_EG[KNIGHT]);
    push_pair(p, THREAT_BY_PAWN_MG[BISHOP], THREAT_BY_PAWN_EG[BISHOP]);
    push_pair(p, THREAT_BY_PAWN_MG[ROOK],   THREAT_BY_PAWN_EG[ROOK]);
    push_pair(p, THREAT_BY_PAWN_MG[QUEEN],  THREAT_BY_PAWN_EG[QUEEN]);
    push_pair(p, THREAT_BY_MINOR_MG[ROOK],  THREAT_BY_MINOR_EG[ROOK]);
    push_pair(p, THREAT_BY_MINOR_MG[QUEEN], THREAT_BY_MINOR_EG[QUEEN]);
    push_pair(p, THREAT_BY_ROOK_MG,         THREAT_BY_ROOK_EG);

    // 13. Outposts
    push_pair(p, KNIGHT_OUTPOST_MG, KNIGHT_OUTPOST_EG);
    push_pair(p, BISHOP_OUTPOST_MG, BISHOP_OUTPOST_EG);
    push_pair(p, ROOK_OUTPOST_MG,   ROOK_OUTPOST_EG);
    push_pair(p, QUEEN_OUTPOST_MG,  QUEEN_OUTPOST_EG);

    // 14. Tempo
    push_pair(p, TEMPO_BONUS, TEMPO_BONUS);

    return p;
}

EvalResult TexelTuner::get_fen_eval_result(const std::string& fen) {
    static std::once_flag init_flag;
    std::call_once(init_flag, []{ SHAYVERI::init_attacks(); });
    SHAYVERI::Board board;
    SHAYVERI::set_from_fen(board, fen);

    // Full engine evaluation, converted to White's perspective.
    int full_eval = SHAYVERI::evaluate(board);
    if (board.side_to_move == SHAYVERI::BLACK) full_eval = -full_eval;

    // Trace the linear features.
    Trace tr              = trace_evaluate(board);
    coefficients_t coeffs = get_coefficients(tr);

    // Compute the linear contribution at the current (initial) parameter values
    // tapered with the position's phase.  This is subtracted from the full eval
    // to obtain additional_score, which captures king safety, development,
    // value-weighted hanging/undefended penalties, and the openness-multiplier
    // effect on mobility.
    static const parameters_t initial = TexelTuner::get_initial_parameters();
    int phase = phase_of(board);

    tune_t lin_mg = 0, lin_eg = 0;
    for (std::size_t i = 0; i < coeffs.size(); ++i) {
        lin_mg += static_cast<tune_t>(coeffs[i]) * initial[i][0];
        lin_eg += static_cast<tune_t>(coeffs[i]) * initial[i][1];
    }
    tune_t lin_tapered = (lin_mg * phase + lin_eg * (MAX_PHASE - phase))
                         / static_cast<tune_t>(MAX_PHASE);

    EvalResult result;
    result.coefficients  = std::move(coeffs);
    result.score         = static_cast<tune_t>(full_eval) - lin_tapered;
    result.endgame_scale = 1.0;
    return result;
}

EvalResult TexelTuner::get_external_eval_result(const chess::Board& board) {
    (void)board;
    return EvalResult{};
}

// print_parameters
// Prints tuned values in tune.h format so they can be pasted back directly.
void TexelTuner::print_parameters(const parameters_t& p) {
    std::stringstream ss;

    int i = 0;
    auto mg = [&](int idx) { return static_cast<int>(std::round(p[idx][0])); };
    auto eg = [&](int idx) { return static_cast<int>(std::round(p[idx][1])); };

    // Material
    ss << "// Piece values MG\n";
    ss << "inline int PIECE_VALUES_MG[PIECE_COUNT] = {\n    0,";
    ss << "   100,";
    for (int pt = 0; pt < 4; ++pt) ss << " " << mg(i + pt) << ",";
    ss << " 0,\n    ";
    ss << "   100,";
    for (int pt = 0; pt < 4; ++pt) ss << " " << mg(i + pt) << ",";
    ss << " 0,\n};\n";

    ss << "inline int PIECE_VALUES_EG[PIECE_COUNT] = {\n    0,";
    ss << "   100,";
    for (int pt = 0; pt < 4; ++pt) ss << " " << eg(i + pt) << ",";
    ss << " 0,\n    ";
    ss << "   100,";
    for (int pt = 0; pt < 4; ++pt) ss << " " << eg(i + pt) << ",";
    ss << " 0,\n};\n";
    i += 4;

    // PST
    static const char* pst_names[] = {
        "PST_PAWN", "PST_KNIGHT", "PST_BISHOP", "PST_ROOK", "PST_QUEEN", "PST_KING"
    };
    for (int pt = 0; pt < 6; ++pt) {
        ss << "inline int " << pst_names[pt] << "_MG[64] = {\n";
        for (int r = 7; r >= 0; --r) {
            ss << "   ";
            for (int f = 0; f < 8; ++f) {
                int sq = r * 8 + f;
                ss << " " << std::setw(4) << mg(i + sq) << ",";
            }
            ss << "\n";
        }
        ss << "};\n";
        ss << "inline int " << pst_names[pt] << "_EG[64] = {\n";
        for (int r = 7; r >= 0; --r) {
            ss << "   ";
            for (int f = 0; f < 8; ++f) {
                int sq = r * 8 + f;
                ss << " " << std::setw(4) << eg(i + sq) << ",";
            }
            ss << "\n";
        }
        ss << "};\n";
        i += 64;
    }

    // Bishop pair
    ss << "inline int BISHOP_PAIR_BONUS = " << mg(i) << "; // EG=" << eg(i) << "\n";
    i++;

    // Pawn structure
    ss << "inline int ISOLATED_PAWN_PENALTY_MG = " << mg(i)   << ";\n";
    ss << "inline int ISOLATED_PAWN_PENALTY_EG = " << eg(i++)  << ";\n";
    ss << "inline int DOUBLED_PAWN_PENALTY_MG  = " << mg(i)   << ";\n";
    ss << "inline int DOUBLED_PAWN_PENALTY_EG  = " << eg(i++)  << ";\n";
    ss << "inline int BACKWARD_PAWN_PENALTY_MG = " << mg(i)   << ";\n";
    ss << "inline int BACKWARD_PAWN_PENALTY_EG = " << eg(i++)  << ";\n";
    ss << "inline int SUPPORTED_PAWN_BONUS_MG  = " << mg(i++)  << ";\n";
    ss << "inline int WEAK_PAWN_PENALTY_MG     = " << mg(i++)  << ";\n";
    ss << "inline int PAWN_ISLAND_PENALTY_MG   = " << mg(i)   << ";\n";
    ss << "inline int PAWN_ISLAND_PENALTY_EG   = " << eg(i++)  << ";\n";
    ss << "inline int PAWN_CENTER_BONUS_MG     = " << mg(i)   << ";\n";
    ss << "inline int PAWN_CENTER_BONUS_EG     = " << eg(i++)  << ";\n";
    ss << "inline int PAWN_EXT_CENTER_BONUS_MG = " << mg(i)   << ";\n";
    ss << "inline int PAWN_EXT_CENTER_BONUS_EG = " << eg(i++)  << ";\n";

    // Pawn storm
    ss << "inline int PAWN_STORM_BASE      = " << mg(i++) << ";\n";
    ss << "inline int PAWN_STORM_RANK_MULT = " << mg(i++) << ";\n";

    // Passed pawns
    ss << "inline int PASSED_PAWN_BONUS_MG[8] = { 0,";
    for (int r = 0; r < 6; ++r) ss << " " << mg(i + r) << ",";
    ss << " 0 };\n";
    ss << "inline int PASSED_PAWN_BONUS_EG[8] = { 0,";
    for (int r = 0; r < 6; ++r) ss << " " << eg(i + r) << ",";
    ss << " 0 };\n";
    i += 6;

    ss << "inline int CANDIDATE_PAWN_BONUS_MG   = " << mg(i)   << ";\n";
    ss << "inline int CANDIDATE_PAWN_BONUS_EG   = " << eg(i++) << ";\n";
    ss << "inline int CONNECTED_PASSED_BONUS_MG = " << mg(i)   << ";\n";
    ss << "inline int CONNECTED_PASSED_BONUS_EG = " << eg(i++) << ";\n";
    ss << "inline int OUTSIDE_PASSED_BONUS_MG   = " << mg(i)   << ";\n";
    ss << "inline int OUTSIDE_PASSED_BONUS_EG   = " << eg(i++) << ";\n";

    // Mobility
    ss << "inline int MOBILITY_KNIGHT_MG = " << mg(i) << ";\n";
    ss << "inline int MOBILITY_KNIGHT_EG = " << eg(i++) << ";\n";
    ss << "inline int MOBILITY_BISHOP_MG = " << mg(i) << ";\n";
    ss << "inline int MOBILITY_BISHOP_EG = " << eg(i++) << ";\n";
    ss << "inline int MOBILITY_ROOK_MG   = " << mg(i) << ";\n";
    ss << "inline int MOBILITY_ROOK_EG   = " << eg(i++) << ";\n";
    ss << "inline int MOBILITY_QUEEN_MG  = " << mg(i) << ";\n";
    ss << "inline int MOBILITY_QUEEN_EG  = " << eg(i++) << ";\n";

    // Territory
    ss << "inline int CENTER_BONUS          = " << mg(i) << "; // EG=" << eg(i) << "\n"; i++;
    ss << "inline int EXT_CENTER_BONUS      = " << mg(i) << "; // EG=" << eg(i) << "\n"; i++;
    ss << "inline int ENEMY_HALF_BONUS      = " << mg(i) << "; // EG=" << eg(i) << "\n"; i++;
    ss << "inline int SEVENTH_RANK_BONUS_MG = " << mg(i) << ";\n";
    ss << "inline int SEVENTH_RANK_BONUS_EG = " << eg(i++) << ";\n";

    // Coordination
    ss << "inline int DEFENDED_PIECE_BONUS       = " << mg(i++) << ";\n";
    ss << "inline int SHARED_TARGET_BONUS        = " << mg(i++) << ";\n";
    ss << "inline int BATTERY_ROOK_QUEEN_BONUS   = " << mg(i++) << ";\n";
    ss << "inline int BATTERY_BISHOP_QUEEN_BONUS = " << mg(i++) << ";\n";
    ss << "inline int SUPPORT_CHAIN_BONUS        = " << mg(i++) << ";\n";

    // Tactical
    ss << "inline int PIN_BONUS                      = " << mg(i++) << ";\n";
    ss << "inline int OVERLOADED_DEFENDER_BONUS      = " << mg(i++) << ";\n";
    ss << "inline int UNRECIPROCATED_PRESSURE_BONUS  = " << mg(i++) << ";\n";

    // Threats
    ss << "inline int THREAT_BY_PAWN_MG[7] = { 0, 0,"
       << " " << mg(i) << ", " << mg(i+1) << ", " << mg(i+2) << ", " << mg(i+3)
       << ", 0 };\n";
    ss << "inline int THREAT_BY_PAWN_EG[7] = { 0, 0,"
       << " " << eg(i) << ", " << eg(i+1) << ", " << eg(i+2) << ", " << eg(i+3)
       << ", 0 };\n";
    i += 4;
    ss << "inline int THREAT_BY_MINOR_MG[7] = { 0, 0, 0, 0,"
       << " " << mg(i) << ", " << mg(i+1) << ", 0 };\n";
    ss << "inline int THREAT_BY_MINOR_EG[7] = { 0, 0, 0, 0,"
       << " " << eg(i) << ", " << eg(i+1) << ", 0 };\n";
    i += 2;
    ss << "inline int THREAT_BY_ROOK_MG = " << mg(i) << ";\n";
    ss << "inline int THREAT_BY_ROOK_EG = " << eg(i++) << ";\n";

    // Outposts
    ss << "inline int KNIGHT_OUTPOST_MG = " << mg(i) << ";\n";
    ss << "inline int KNIGHT_OUTPOST_EG = " << eg(i++) << ";\n";
    ss << "inline int BISHOP_OUTPOST_MG = " << mg(i) << ";\n";
    ss << "inline int BISHOP_OUTPOST_EG = " << eg(i++) << ";\n";
    ss << "inline int ROOK_OUTPOST_MG   = " << mg(i) << ";\n";
    ss << "inline int ROOK_OUTPOST_EG   = " << eg(i++) << ";\n";
    ss << "inline int QUEEN_OUTPOST_MG  = " << mg(i) << ";\n";
    ss << "inline int QUEEN_OUTPOST_EG  = " << eg(i++) << ";\n";

    // Tempo
    ss << "inline int TEMPO_BONUS = " << mg(i++) << ";\n";

    std::cout << ss.str() << std::flush;
}
