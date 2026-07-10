#include "search.h"

#include "attacks.h"
#include "evaluate.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "nnue.h"
#include "nnue_update.h"
#include "see.h"
#include "tt.h"
#include "tune.h"
#include "zobrist.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>

#include <cctype>
#include <climits>
#include <cmath>
#include <cstring>

namespace SHAYVERI {

std::atomic<bool> g_stop = false;
std::atomic<U64> node_count = 0;
std::atomic<U64> node_limit = 0;

using namespace Tune;

thread_local bool local_node_limited_search = false;
thread_local bool local_stop = false;
thread_local U64 local_node_count = 0;
thread_local U64 local_node_limit = 0;

static inline int lmr_reduction_base(int depth, int moves) {
    if (depth < 2 || moves < 2) return 0;
    const double d = static_cast<double>(depth);
    const double m = static_cast<double>(moves);
    const double scale = std::max(0.01, Tune::lmr_scale);
    return static_cast<int>(Tune::lmr_base + std::log(d) * std::log(m) / scale);
}

struct ScoredMove {
    Move m;
    int score;
};

// Tracks per-ply state for histories and extensions.
struct StackInfo {
    Move move;
    Piece piece;
    Move excluded_move;
    NNUE::Accumulator acc;
};

struct SearchHeuristics {
    Move killers[MAX_PLY][2];
    Move counter_moves[7][64];
    int history[2][64][64]; // [side_to_move][from][to]

    // Continuation histories use PieceType indices 1=PAWN..6=KING.
    int counter_move_history[7][64][7][64]; // [prev_pt][prev_to][curr_pt][curr_to]
    int follow_up_history[7][64][7][64];    // [prev2_pt][prev2_to][curr_pt][curr_to]

    // Capture history, indexed by [attacker_pt][to_sq][captured_pt].
    int capture_history[7][64][7];

    // Static eval correction history, indexed by side and pawn-structure key.
    int correction_history[2][Tune::CORRHIST_TABLE_SIZE];

    SearchHeuristics() {
        std::memset(killers, 0, sizeof(killers));
        std::memset(counter_moves, 0, sizeof(counter_moves));
        std::memset(history, 0, sizeof(history));
        std::memset(counter_move_history, 0, sizeof(counter_move_history));
        std::memset(follow_up_history, 0, sizeof(follow_up_history));
        std::memset(capture_history, 0, sizeof(capture_history));
        std::memset(correction_history, 0, sizeof(correction_history));
    }

    inline void store_killer(int ply, Move m) {
        if (ply < 0 || ply >= MAX_PLY) return;
        if (killers[ply][0] == m) return;
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = m;
    }

    // Gravity-based, SPSA-friendly history update (branchless abs)
    inline void update_history(int& entry, int bonus) {
        int abs_bonus = bonus < 0 ? -bonus : bonus;
        entry += bonus - entry * abs_bonus / Tune::history_max;
    }
};

static SearchHeuristics persistent_history;
static std::mutex persistent_history_mutex;

static inline int correction_history_index(const Board& b) {
    const U64 wp = b.bit_boards[WP];
    const U64 bp = b.bit_boards[BP];
    const U64 bp_rot = (bp << 32) | (bp >> 32);
    U64 key = wp * 0x9E3779B97F4A7C15ULL;
    key ^= bp_rot * 0xBF58476D1CE4E5B9ULL;
    return static_cast<int>(key & (Tune::CORRHIST_TABLE_SIZE - 1));
}

static inline int corrected_static_eval(const Board& b, int raw_eval, const SearchHeuristics& H) {
    const int stm = static_cast<int>(b.side_to_move) & 1;
    const int entry = H.correction_history[stm][correction_history_index(b)];
    return raw_eval + entry / std::max(1, Tune::corrhist_scale);
}

static inline void update_correction_history(const Board& b, SearchHeuristics& H,
                                             int raw_eval, int score, int depth, int bound) {
    if (std::abs(score) >= MATE_SCORE - MAX_PLY) return;

    const bool useful =
        bound == TT_EXACT ||
        (bound == TT_LOWER && score > raw_eval) ||
        (bound == TT_UPPER && score < raw_eval);
    if (!useful) return;

    const int stm = static_cast<int>(b.side_to_move) & 1;
    int& entry = H.correction_history[stm][correction_history_index(b)];
    const int max_entry = std::max(1, Tune::corrhist_max);
    const int diff = std::clamp(score - raw_eval, -max_entry, max_entry);
    const int depth_cap = std::max(1, Tune::corrhist_depth_cap);
    const int depth_weight = std::min(depth, depth_cap);
    const int bonus_limit = std::max(1, Tune::corrhist_bonus_limit);
    const int bonus = std::clamp(diff * Tune::corrhist_bonus_mult * depth_weight / depth_cap,
                                 -bonus_limit, bonus_limit);
    int abs_bonus = bonus < 0 ? -bonus : bonus;
    entry += bonus - entry * abs_bonus / max_entry;
    entry = std::clamp(entry, -max_entry, max_entry);
}

void clear_search_histories() {
    std::lock_guard<std::mutex> lock(persistent_history_mutex);
    persistent_history = SearchHeuristics{};
}

static inline bool search_stopped() {
    return local_node_limited_search ? local_stop : g_stop.load(std::memory_order_relaxed);
}

static inline void request_search_stop() {
    if (local_node_limited_search)
        local_stop = true;
    else
        g_stop.store(true, std::memory_order_relaxed);
}

static inline U64 searched_nodes() {
    return local_node_limited_search ? local_node_count : node_count.load(std::memory_order_relaxed);
}

static inline void count_node() {
    if (local_node_limited_search) {
        ++local_node_count;
        if (local_node_limit != 0 && local_node_count >= local_node_limit)
            local_stop = true;
        return;
    }

    U64 nodes = node_count.fetch_add(1, std::memory_order_relaxed) + 1;
    U64 limit = node_limit.load(std::memory_order_relaxed);
    if (limit != 0 && nodes >= limit)
        request_search_stop();
}

std::string move_to_uci(Move m) {
    if (m == MOVE_NONE) return "0000";

    auto sq_to_str = [](Square s) -> std::string {
        std::string r;
        r += static_cast<char>('a' + get_file(s));
        r += static_cast<char>('1' + get_rank(s));
        return r;
    };
    std::string s = sq_to_str(move_from(m)) + sq_to_str(move_to(m));
    PieceType promo = move_promo(m);
    if (promo != NONE_PTYPE) {
        char pc = 0;
        switch (promo) {
            case KNIGHT: pc = 'n'; break;
            case BISHOP: pc = 'b'; break;
            case ROOK:   pc = 'r'; break;
            case QUEEN:  pc = 'q'; break;
            default: break;
        }
        if (pc) s += pc;
    }
    return s;
}

static bool is_uci_square_text(const std::string& uci, int offset) {
    return uci.size() > static_cast<size_t>(offset + 1) &&
           uci[offset] >= 'a' && uci[offset] <= 'h' &&
           uci[offset + 1] >= '1' && uci[offset + 1] <= '8';
}

static bool is_uci_move_text(const std::string& uci) {
    if (uci.size() != 4 && uci.size() != 5) return false;
    if (!is_uci_square_text(uci, 0) || !is_uci_square_text(uci, 2)) return false;
    if (uci.size() == 5) {
        char promo = static_cast<char>(std::tolower(static_cast<unsigned char>(uci[4])));
        return promo == 'n' || promo == 'b' || promo == 'r' || promo == 'q';
    }
    return true;
}

Move uci_to_move(Board& b, const std::string& uci) {
    if (!is_uci_move_text(uci)) return MOVE_NONE;
    File ff = File(uci[0] - 'a');
    Rank fr = Rank(uci[1] - '1');
    File tf = File(uci[2] - 'a');
    Rank tr = Rank(uci[3] - '1');
    Square from = make_square(ff, fr);
    Square to = make_square(tf, tr);

    PieceType promo = NONE_PTYPE;
    if (uci.size() == 5) {
        switch (std::tolower(static_cast<unsigned char>(uci[4]))) {
            case 'n': promo = KNIGHT; break;
            case 'b': promo = BISHOP; break;
            case 'r': promo = ROOK; break;
            case 'q': promo = QUEEN; break;
            default: break;
        }
    }

    MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i) {
        Move m = legal.moves[i];
        if (move_from(m) == from && move_to(m) == to && move_promo(m) == promo)
            return m;
    }
    return MOVE_NONE;
}

static inline bool has_non_pawn_material(const Board &b, Colour c) {
    const int off = (c == WHITE) ? 0 : 6;
    return (b.bit_boards[Piece(off + WN)] |
            b.bit_boards[Piece(off + WB)] |
            b.bit_boards[Piece(off + WR)] |
            b.bit_boards[Piece(off + WQ)]) != 0;
}

void make_null_move(Board &b, Undo &u) {
    u.hash       = b.hash;
    u.en_passant = b.en_passant;
    u.half_move  = b.half_move;
    u.castling   = b.castling;
    u.captured   = NONE_PIECE;
    u.was_ep     = false;
    u.was_castle = false;

    if (b.en_passant != SQ_NONE) {
        b.hash ^= Zobrist::en_passants[get_file(b.en_passant)];
        b.en_passant = SQ_NONE;
    }

    b.side_to_move = flip(b.side_to_move);
    b.hash ^= Zobrist::sides;
    b.half_move++;
}

void unmake_null_move(Board &b, const Undo &u) {
    b.side_to_move = flip(b.side_to_move);
    b.en_passant   = u.en_passant;
    b.half_move    = u.half_move;
    b.castling     = u.castling;
    b.hash         = u.hash;
}

static inline int capture_order_score(const Board& b, Move m) {
    if (move_promo(m) != NONE_PTYPE)
        return 1000000;

    PieceType victim = NONE_PTYPE;
    if (is_ep_move(m)) {
        victim = PAWN;
    } else {
        Piece vp = b.get_piece(move_to(m));
        if (vp == NONE_PIECE) return 0;
        victim = get_type(vp);
    }

    PieceType attacker = get_type(b.get_piece(move_from(m)));
    return 100000 + (10 * PTYPE_VALUES[victim]) - PTYPE_VALUES[attacker];
}

static inline bool is_capture_or_promo(const Board& b, Move m) {
    if (move_promo(m) != NONE_PTYPE) return true;
    if (is_ep_move(m)) return true;
    return b.get_piece(move_to(m)) != NONE_PIECE;
}

static inline int order_score(const Board &b, Move m, int ply, const SearchHeuristics &H, StackInfo* ss) {
    int cap = capture_order_score(b, m);
    if (cap != 0) {
        int see_val = see(b, m);
        // Add capture history bonus for non-promotion captures to fine-tune ordering
        int ch = 0;
        if (move_promo(m) == NONE_PTYPE) {
            int to = static_cast<int>(move_to(m)) & 63;
            int attacker_pt = static_cast<int>(get_type(b.get_piece(move_from(m))));
            PieceType victim_pt = is_ep_move(m) ? PAWN : get_type(b.get_piece(to));
            if (victim_pt != NONE_PTYPE)
                ch = H.capture_history[attacker_pt][to][static_cast<int>(victim_pt)]
                     * Tune::capture_history_weight / 100;
        }
        if (see_val >= 0) return 1000000 + (see_val * 100) + cap + ch;
        return 700000 + (see_val * 100) + cap + ch;
    }

    if (ply >= 0 && ply < MAX_PLY) {
        if (m == H.killers[ply][0]) return 900000;
        if (m == H.killers[ply][1]) return 890000;
    }

    if (ply >= 1 && ss[-1].move != MOVE_NONE && ss[-1].piece != NONE_PIECE) {
        int prev_pt = static_cast<int>(get_type(ss[-1].piece));
        int prev_to = static_cast<int>(move_to(ss[-1].move)) & 63;
        if (m == H.counter_moves[prev_pt][prev_to]) return 880000;
    }

    int pt  = static_cast<int>(get_type(b.get_piece(move_from(m))));
    int to  = static_cast<int>(move_to(m)) & 63;
    int stm = static_cast<int>(b.side_to_move) & 1;

    int history_score = H.history[stm][static_cast<int>(move_from(m))][to] * Tune::main_history_weight;

    // Check bounds for previous plies
    if (ply >= 1 && ss[-1].move != MOVE_NONE) {
        int prev_pt = static_cast<int>(get_type(ss[-1].piece));
        int prev_to = static_cast<int>(move_to(ss[-1].move)) & 63;
        history_score += H.counter_move_history[prev_pt][prev_to][pt][to] * Tune::cmh_weight;
    }
    if (ply >= 2 && ss[-2].move != MOVE_NONE) {
        int prev2_pt = static_cast<int>(get_type(ss[-2].piece));
        int prev2_to = static_cast<int>(move_to(ss[-2].move)) & 63;
        history_score += H.follow_up_history[prev2_pt][prev2_to][pt][to] * Tune::fmh_weight;
    }

    return history_score / 100;
}

static inline int quiet_history_score(const Board &b, Move m, int ply, const SearchHeuristics &H, StackInfo* ss) {
    int pt  = static_cast<int>(get_type(b.get_piece(move_from(m))));
    int to  = static_cast<int>(move_to(m)) & 63;
    int stm = static_cast<int>(b.side_to_move) & 1;

    int score = H.history[stm][static_cast<int>(move_from(m))][to] * Tune::main_history_weight;

    if (ply >= 1 && ss[-1].move != MOVE_NONE) {
        int prev_pt = static_cast<int>(get_type(ss[-1].piece));
        int prev_to = static_cast<int>(move_to(ss[-1].move)) & 63;
        score += H.counter_move_history[prev_pt][prev_to][pt][to] * Tune::cmh_weight;
    }
    if (ply >= 2 && ss[-2].move != MOVE_NONE) {
        int prev2_pt = static_cast<int>(get_type(ss[-2].piece));
        int prev2_to = static_cast<int>(move_to(ss[-2].move)) & 63;
        score += H.follow_up_history[prev2_pt][prev2_to][pt][to] * Tune::fmh_weight;
    }

    return score / 100;
}

// Step by 2 (same side to move), limit search to last half_move plies.
static inline bool is_repetition(U64 key, const U64* rep_stack, int rep_len, int half_move) {
    int limit = rep_len - half_move;
    if (limit < 0) limit = 0;
    for (int i = rep_len - 2; i >= limit; i -= 2)
        if (rep_stack[i] == key) return true;
    return false;
}

static inline int evaluate_position(const Board &b, const StackInfo *ss) {
    if (NNUE::is_enabled())
        return NNUE::evaluate(static_cast<int>(b.side_to_move), ss->acc);
    return evaluate(b);
}

static int qsearch(Board &b, int alpha, int beta, int depth, int ply, StackInfo *ss) {
    if (search_stopped()) return 0;
    count_node();

    alpha = std::max(alpha, -MATE_SCORE + ply);
    beta = std::min(beta, MATE_SCORE - ply - 1);
    if (alpha >= beta) return alpha;

    const TTEntry *tte = tt().probe(b.hash);

    // TT probe: use any stored result for immediate cutoffs
    if (tte && tte->depth >= 0) {
        const TTEntry *e = tte;
        int s = e->score;
        if (s > MATE_SCORE - MAX_PLY) s -= ply;
        else if (s < -MATE_SCORE + MAX_PLY) s += ply;
        if (e->flag == TT_EXACT)               return s;
        if (e->flag == TT_LOWER && s >= beta)  return s;
        if (e->flag == TT_UPPER && s <= alpha) return s;
    }

    Square ksq    = king_square(b, b.side_to_move);
    bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));

    int stand_pat = 0;
    if (!in_check) {
        if (tte && tte->has_eval) {
            stand_pat = tte->eval;
        } else {
            stand_pat = evaluate_position(b, ss);
            tt().store_eval(b.hash, stand_pat);
        }
        if (stand_pat >= beta) return stand_pat;
        if (stand_pat > alpha) alpha = stand_pat;
        if (depth <= 0) return alpha;
    } else {
        if (depth < Tune::qsearch_min_depth) return evaluate_position(b, ss);
    }

    MoveList moves;
    ScoredMove noisy[256];
    int noisy_count = 0;

    if (in_check) {
        // In check: search all pseudo-legal moves
        moves = generate_pseudo_legal_moves(b);
        for (int i = 0; i < moves.count; ++i) {
            Move m = moves.moves[i];
            int s  = capture_order_score(b, m);
            noisy[noisy_count++] = {m, s};
        }
    } else {
        // Not in check: only captures and promotions
        moves = generate_pseudo_legal_captures(b);
        for (int i = 0; i < moves.count; ++i)
            noisy[noisy_count++] = {moves.moves[i], capture_order_score(b, moves.moves[i])};
    }

    int legal_count = 0;

    for (int i = 0; i < noisy_count; ++i) {
        // Lazy selection sort: pick the best remaining move
        for (int j = i + 1; j < noisy_count; ++j)
            if (noisy[j].score > noisy[i].score) std::swap(noisy[i], noisy[j]);
        Move m = noisy[i].m;

        if (!in_check) {
            if (!is_promo(m) && b.get_piece(move_to(m)) != NONE_PIECE && !see_ge_zero(b, m))
                continue;

            Piece vp = b.get_piece(move_to(m));
            int victim_val = (vp != NONE_PIECE) ? PTYPE_VALUES[get_type(vp)] : (is_ep_move(m) ? PTYPE_VALUES[PAWN] : 0);
            if (move_promo(m) != NONE_PTYPE) victim_val += PTYPE_VALUES[move_promo(m)];

            if (stand_pat + victim_val + Tune::qs_delta_margin < alpha)
                continue;
        }

        Undo u;
        NNUE::update_accumulator((ss + 1)->acc, ss->acc, b, m);
        if (!make_move(b, m, u)) continue;
        legal_count++;

        int score = -qsearch(b, -beta, -alpha, depth - 1, ply + 1, ss + 1);
        unmake_move(b, m, u);

        if (search_stopped()) return 0;

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    if (in_check && legal_count == 0) return -MATE_SCORE + ply;

    return alpha;
}

static int negamax(Board &b, int depth, int alpha, int beta, int ply,
                   SearchHeuristics &H, U64 *rep_stack, int rep_len, bool pv_node, StackInfo* ss,
                   bool allow_null = true) {
    if (search_stopped()) return 0;
    count_node();

    const U64 key = b.hash;

    // Basic Draw Detection
    if (b.half_move >= 100) return 0;
    if (rep_len > 1 && is_repetition(key, rep_stack, rep_len - 1, b.half_move)) return 0;

    alpha = std::max(alpha, -MATE_SCORE + ply);
    beta = std::min(beta, MATE_SCORE - ply - 1);
    if (alpha >= beta) return alpha;

    const int original_alpha = alpha;
    Move tt_move = MOVE_NONE;
    int tt_score = 0;
    int tt_depth = -1;
    int tt_bound = -1; // -1 represents "No entry found"
    bool tt_has_eval = false;
    int tt_eval = 0;

    if (const TTEntry *e = tt().probe(key)) {
        tt_move  = e->best;
        tt_score = e->score;
        tt_depth = e->depth;
        tt_bound = e->flag;
        tt_has_eval = e->has_eval;
        tt_eval = e->eval;

        if (tt_depth >= depth) {
            int s = tt_score;
            if (s > MATE_SCORE - MAX_PLY) s -= ply;
            else if (s < -MATE_SCORE + MAX_PLY) s += ply;

            // Only prune if we are not in a singular extension check
            if (!pv_node && ss->excluded_move == MOVE_NONE) {
                if (tt_bound == TT_EXACT) return s;
                if (tt_bound == TT_LOWER) alpha = std::max(alpha, s);
                else if (tt_bound == TT_UPPER) beta = std::min(beta, s);
                if (alpha >= beta) return s;
            }
        }
    }

    if (depth <= 0) return qsearch(b, alpha, beta, Tune::qsearch_start_depth, ply, ss);

    Square ksq    = king_square(b, b.side_to_move);
    bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));
    if (in_check && ply < MAX_PLY - 1) depth++;

    int raw_static_eval = 0;
    int static_eval = 0;
    bool have_static_eval = false;
    if (!in_check) {
        if (tt_has_eval) {
            raw_static_eval = tt_eval;
        } else {
            raw_static_eval = evaluate_position(b, ss);
            tt().store_eval(key, raw_static_eval);
        }
        static_eval = corrected_static_eval(b, raw_static_eval, H);
        have_static_eval = true;
    }

    // Reverse Futility Pruning
    if (!pv_node && !in_check && depth <= Tune::rfp_max_depth && ss->excluded_move == MOVE_NONE) {
        if (static_eval - (Tune::rfp_margin_mult * depth) >= beta)
            return static_eval;
    }

    // Null Move Pruning
    if (allow_null && !pv_node && !in_check && depth >= Tune::nmp_min_depth
        && static_eval >= beta + Tune::nmp_margin_mult * depth
        && has_non_pawn_material(b, b.side_to_move) && ss->excluded_move == MOVE_NONE) {
        const int eval_divisor = std::max(1, Tune::nmp_eval_divisor);
        const int depth_divisor = std::max(1, Tune::nmp_depth_divisor);
        const int reduction_min = std::min(Tune::nmp_reduction_min, Tune::nmp_reduction_max);
        const int reduction_max = std::max(Tune::nmp_reduction_min, Tune::nmp_reduction_max);
        const int R = std::clamp(Tune::nmp_base_reduction + depth / depth_divisor +
                                 (static_eval - beta) / eval_divisor,
                                 reduction_min, reduction_max);
        Undo u;
        (ss + 1)->acc = ss->acc;
        make_null_move(b, u);
        ss->move = MOVE_NONE;
        ss->piece = NONE_PIECE;
        int score = -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, H, rep_stack, rep_len, false, ss + 1);
        unmake_null_move(b, u);

        if (search_stopped()) return 0;
        if (score >= beta) {
            if (depth >= Tune::nmp_verify_min_depth) {
                int verify = negamax(b, depth - R, beta - 1, beta, ply, H, rep_stack, rep_len, false, ss, false);
                if (search_stopped()) return 0;
                if (verify < beta) {
                    // Zugzwang-ish high-depth fail highs must survive verification.
                } else {
                    return score;
                }
            } else {
                return score;
            }
        }
    }

    // Singular Extensions
    int extension = 0;
    if (!pv_node && !in_check && ss->excluded_move == MOVE_NONE && depth >= Tune::se_min_depth
        && tt_move != MOVE_NONE && tt_bound != TT_UPPER && tt_depth >= depth - Tune::se_depth_margin
        && std::abs(tt_score) < MATE_SCORE - MAX_PLY) {

        int r_beta = tt_score - Tune::se_margin;
        int se_depth = (depth - 1) / Tune::se_reduction_denom;

        ss->excluded_move = tt_move;
        int score = negamax(b, se_depth, r_beta - 1, r_beta, ply, H, rep_stack, rep_len, false, ss);
        ss->excluded_move = MOVE_NONE;

        if (score < r_beta) extension = 1;
    }

    // IIR: when no TT move exists, do a cheaper search by reducing depth by 1.
    // This encourages us to search a reduced-depth iteration first, which then
    // populates the TT for a subsequent re-search at full depth.
    if (depth >= Tune::iir_min_depth && tt_move == MOVE_NONE && ss->excluded_move == MOVE_NONE) {
        depth--;
    }

    MoveList pseudo = generate_pseudo_legal_moves(b);
    ScoredMove ordered[256];
    for (int i = 0; i < pseudo.count; ++i) {
        Move m = pseudo.moves[i];
        int s  = order_score(b, m, ply, H, ss);
        if (tt_move != MOVE_NONE && m == tt_move) s += 2000000;
        ordered[i] = {m, s};
    }
    int ordered_count = pseudo.count;

    Move best_move = MOVE_NONE;
    int legal_count = 0;
    int quiet_count = 0;
    Move quiets_played[256];

    // Track non-promotion captures tried, for capture history updates
    struct CaptureEntry { Move m; PieceType captured_pt; PieceType attacker_pt; };
    CaptureEntry captures_tried[256];
    int capture_tried_count = 0;

    for (int i = 0; i < ordered_count; ++i) {
        // Lazy selection sort: swap best remaining move to position i
        for (int j = i + 1; j < ordered_count; ++j)
            if (ordered[j].score > ordered[i].score) std::swap(ordered[i], ordered[j]);

        Move m = ordered[i].m;
        if (m == ss->excluded_move) continue;

        bool is_quiet = !is_capture_or_promo(b, m);

        // Pruning (LMP, Futility)
        if (!pv_node && !in_check && ss->excluded_move == MOVE_NONE) {
            if (is_quiet && legal_count > (Tune::lmp_base + Tune::lmp_mult * depth * depth)) continue;
            if (is_quiet && depth <= Tune::futility_max_depth && static_eval + Tune::fp_base + Tune::fp_mult * depth <= alpha) continue;
            if (!is_quiet && move_promo(m) == NONE_PTYPE && depth <= Tune::see_pruning_max_depth &&
                see(b, m) < Tune::see_pruning_margin * depth)
                continue;
        }

        Piece moved_piece = b.get_piece(move_from(m));

        // Pre-compute victim piece type for capture history (before make_move modifies the board)
        PieceType cap_victim_pt = NONE_PTYPE;
        if (!is_quiet && move_promo(m) == NONE_PTYPE) {
            cap_victim_pt = is_ep_move(m) ? PAWN : get_type(b.get_piece(move_to(m)));
        }

        Undo u;
        NNUE::update_accumulator((ss + 1)->acc, ss->acc, b, m);
        if (!make_move(b, m, u)) continue;

        legal_count++;
        if (is_quiet) quiets_played[quiet_count++] = m;
        else if (cap_victim_pt != NONE_PTYPE) captures_tried[capture_tried_count++] = {m, cap_victim_pt, get_type(moved_piece)};
        rep_stack[rep_len] = b.hash;
        ss->move = m;
        ss->piece = moved_piece;

        int score;
        int d_ext = (m == tt_move) ? extension : 0;

        // PVS Logic with LMR
        if (legal_count == 1) {
            score = -negamax(b, depth - 1 + d_ext, -beta, -alpha, ply + 1, H, rep_stack, rep_len + 1, pv_node, ss + 1);
        } else {
            int reduction = 0;
            if (depth >= Tune::lmr_min_depth && is_quiet && m != tt_move && m != H.killers[ply][0]) {
                reduction = lmr_reduction_base(depth, legal_count);
                if (!pv_node) reduction++;
                if (pv_node) reduction--;
                if (legal_count > Tune::lmr_extra_move_threshold && depth >= Tune::lmr_extra_min_depth) reduction++;
                int hist = quiet_history_score(b, m, ply, H, ss);
                if (hist > Tune::lmr_good_history_threshold) reduction--;
                else if (hist < Tune::lmr_bad_history_threshold) reduction++;
                reduction = std::clamp(reduction, 0, depth - 2);
            }

            score = -negamax(b, depth - 1 - reduction + d_ext, -alpha - 1, -alpha, ply + 1, H, rep_stack, rep_len + 1, false, ss + 1);
            if (score > alpha && reduction > 0)
                score = -negamax(b, depth - 1 + d_ext, -alpha - 1, -alpha, ply + 1, H, rep_stack, rep_len + 1, false, ss + 1);
            if (pv_node && score > alpha && score < beta)
                score = -negamax(b, depth - 1 + d_ext, -beta, -alpha, ply + 1, H, rep_stack, rep_len + 1, true, ss + 1);
        }

        unmake_move(b, m, u);
        if (search_stopped()) return 0;

        if (score >= beta) {
            if (have_static_eval && !in_check && ss->excluded_move == MOVE_NONE)
                update_correction_history(b, H, raw_static_eval, score, depth, TT_LOWER);
            if (ss->excluded_move == MOVE_NONE) {
                int tt_s = (score > MATE_SCORE - MAX_PLY) ? score + ply : (score < -MATE_SCORE + MAX_PLY ? score - ply : score);
                tt().store(key, depth, tt_s, TT_LOWER, m);
            }
            if (is_quiet) {
                H.store_killer(ply, m);
                int bonus = std::min(Tune::history_bonus_limit, Tune::history_bonus_mult * depth - Tune::history_bonus_sub);

                auto update_all = [&](Move move, int pt_idx, int bns) {
                    int f = move_from(move);
                    int t = move_to(move);
                    H.update_history(H.history[static_cast<int>(b.side_to_move)][f][t], bns);

                    if (ss[-1].move != MOVE_NONE) {
                        int p1 = static_cast<int>(get_type(ss[-1].piece));
                        int t1 = static_cast<int>(move_to(ss[-1].move)) & 63;
                        H.update_history(H.counter_move_history[p1][t1][pt_idx][t], bns);
                    }
                    if (ss[-2].move != MOVE_NONE) {
                        int p2 = static_cast<int>(get_type(ss[-2].piece));
                        int t2 = static_cast<int>(move_to(ss[-2].move)) & 63;
                        H.update_history(H.follow_up_history[p2][t2][pt_idx][t], bns);
                    }
                };

                int moved_pc_idx = static_cast<int>(get_type(moved_piece));
                if (ss[-1].move != MOVE_NONE && ss[-1].piece != NONE_PIECE) {
                    int prev_pt = static_cast<int>(get_type(ss[-1].piece));
                    int prev_to = static_cast<int>(move_to(ss[-1].move)) & 63;
                    H.counter_moves[prev_pt][prev_to] = m;
                }
                update_all(m, moved_pc_idx, bonus);
                for (int j = 0; j < quiet_count - 1; ++j) {
                    int q_pc_idx = static_cast<int>(get_type(b.get_piece(move_from(quiets_played[j]))));
                    update_all(quiets_played[j], q_pc_idx, -bonus);
                }
            } else if (cap_victim_pt != NONE_PTYPE) {
                // Update capture history: reward the cutting capture, penalise prior ones
                int bonus = std::min(Tune::history_bonus_limit, Tune::history_bonus_mult * depth - Tune::history_bonus_sub);
                int attacker_pt = static_cast<int>(get_type(moved_piece));
                int to = static_cast<int>(move_to(m)) & 63;
                H.update_history(H.capture_history[attacker_pt][to][cap_victim_pt], bonus);
                for (int j = 0; j < capture_tried_count - 1; ++j) {
                    int t2 = static_cast<int>(move_to(captures_tried[j].m)) & 63;
                    H.update_history(H.capture_history[captures_tried[j].attacker_pt][t2][captures_tried[j].captured_pt], -bonus);
                }
            }
            return score;
        }

        if (score > alpha) {
            alpha = score;
            best_move = m;
        }
    }

    if (legal_count == 0) return in_check ? -MATE_SCORE + ply : 0;

    if (ss->excluded_move == MOVE_NONE) {
        if (have_static_eval && !in_check) {
            const int bound = (alpha <= original_alpha) ? TT_UPPER : TT_EXACT;
            update_correction_history(b, H, raw_static_eval, alpha, depth, bound);
        }
        int tt_s = (alpha > MATE_SCORE - MAX_PLY) ? alpha + ply : (alpha < -MATE_SCORE + MAX_PLY ? alpha - ply : alpha);
        tt().store(key, depth, tt_s, (alpha <= original_alpha) ? TT_UPPER : TT_EXACT, best_move);
    }

    return alpha;
}

SearchResult search(Board &b, int max_depth, const U64 *rep_init, int rep_init_len,
                    const std::vector<Move> &search_moves, IterCallback on_iter,
                    bool silent, int root_bias) {

    auto H_storage = std::make_unique<SearchHeuristics>();
    SearchHeuristics& H = *H_storage;
    if (!silent && root_bias == 0) {
        std::lock_guard<std::mutex> lock(persistent_history_mutex);
        H = persistent_history;
    }

    auto save_history = [&]() {
        if (!silent && root_bias == 0) {
            std::lock_guard<std::mutex> lock(persistent_history_mutex);
            persistent_history = H;
        }
    };

    // Stack setup to handle history states (allows up to ss[-2] safely at root)
    auto st = std::make_unique<StackInfo[]>(MAX_PLY + 5);
    StackInfo* ss = st.get() + 2;
    ss->acc.refresh(b);

    Move final_best_move  = MOVE_NONE;
    int  final_best_score = -INF;
    int  completed_depth  = 0;

    U64 rep_stack[MAX_PLY * 2];
    int rep_len = 0;

    if (rep_init && rep_init_len > 0) {
        rep_len = std::min(rep_init_len, MAX_PLY);
        for (int i = 0; i < rep_len; ++i) rep_stack[i] = rep_init[i];
    }
    if (rep_len == 0 || rep_stack[rep_len - 1] != b.hash) {
        if (rep_len < MAX_PLY) rep_stack[rep_len++] = b.hash;
        else                   rep_stack[MAX_PLY - 1] = b.hash;
    }

    auto start_time = std::chrono::steady_clock::now();

    for (int depth = 1; depth <= max_depth; ++depth) {
        int window_alpha = -INF;
        int window_beta  = INF;
        int delta = Tune::ASP_DELTA;

        if (depth >= Tune::aspiration_min_depth && std::abs(final_best_score) < MATE_SCORE - MAX_PLY) {
            window_alpha = std::max(-INF, final_best_score - delta);
            window_beta  = std::min(INF, final_best_score + delta);
        }

        // Generate and order root moves once per depth (outside aspiration retry loop).
        MoveList pseudo = generate_pseudo_legal_moves(b);
        Move tt_root = MOVE_NONE;
        if (const TTEntry* e = tt().probe(b.hash)) tt_root = e->best;
        ScoredMove ordered[256];
        for (int i = 0; i < pseudo.count; ++i) {
            Move m = pseudo.moves[i];
            int  s = order_score(b, m, 0, H, ss);
            if (tt_root != MOVE_NONE && m == tt_root) s += 2000000;
            if (root_bias != 0) s += ((i + root_bias) & 7) * 2;
            ordered[i] = {m, s};
        }
        int root_count = pseudo.count;

        while (true) {
            int current_alpha = window_alpha;
            int current_beta  = window_beta;

            int best_score_this_depth = -INF;
            Move best_move_this_depth = MOVE_NONE;

            int legal_root_count = 0;

            for (int i = 0; i < root_count; ++i) {
                for (int j = i + 1; j < root_count; ++j)
                    if (ordered[j].score > ordered[i].score) std::swap(ordered[i], ordered[j]);

                Move m = ordered[i].m;

                if (!search_moves.empty()) {
                    bool found = false;
                    for (Move sm : search_moves) if (sm == m) { found = true; break; }
                    if (!found) continue;
                }

                Undo u;
                Piece root_piece = b.get_piece(move_from(m));
                NNUE::update_accumulator((ss + 1)->acc, ss->acc, b, m);
                if (!make_move(b, m, u)) continue;
                legal_root_count++;
                rep_stack[rep_len] = b.hash;

                ss->move = m;
                ss->piece = root_piece;

                int score;
                if (legal_root_count == 1) {
                    score = -negamax(b, depth - 1, -current_beta, -current_alpha, 1, H, rep_stack, rep_len + 1, true, ss + 1);
                } else {
                    score = -negamax(b, depth - 1, -current_alpha - 1, -current_alpha, 1, H, rep_stack, rep_len + 1, false, ss + 1);
                    if (score > current_alpha && score < current_beta) {
                        score = -negamax(b, depth - 1, -current_beta, -current_alpha, 1, H, rep_stack, rep_len + 1, true, ss + 1);
                    }
                }

                unmake_move(b, m, u);

                if (search_stopped()) break;

                if (score > best_score_this_depth) {
                    best_score_this_depth = score;
                    best_move_this_depth  = m;
                }

                if (score > current_alpha) current_alpha = score;
            }

            if (search_stopped()) break;

            if (legal_root_count == 0) {
                Square ksq = king_square(b, b.side_to_move);
                bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));
                save_history();
                return {MOVE_NONE, MOVE_NONE, in_check ? -MATE_SCORE : 0, 0, 0};
            }

            if (best_score_this_depth <= window_alpha && window_alpha > -INF) {
                window_alpha = std::max(-INF, window_alpha - delta);
                delta *= Tune::aspiration_growth;
                continue;
            } else if (best_score_this_depth >= window_beta && window_beta < INF) {
                window_beta = std::min(INF, window_beta + delta);
                delta *= Tune::aspiration_growth;
                continue;
            }

            final_best_score = best_score_this_depth;
            final_best_move  = best_move_this_depth;
            completed_depth  = depth;
            break;
        }

        if (search_stopped()) break;

        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (ms == 0) ms = 1;
        U64 nodes = searched_nodes();
        U64 nps = (nodes * 1000ULL) / ms;

        std::string pv_line;
        Board b_pv = b;
        Move pv_move = final_best_move;
        U64 pv_rep_stack[MAX_PLY * 2];
        int pv_rep_len = std::min(rep_len, MAX_PLY * 2);
        for (int i = 0; i < pv_rep_len; ++i) pv_rep_stack[i] = rep_stack[i];

        for (int i = 0; i < depth && pv_move != MOVE_NONE; ++i) {
            if (b_pv.half_move >= 100) break;
            if (pv_rep_len > 1 && is_repetition(b_pv.hash, pv_rep_stack, pv_rep_len - 1, b_pv.half_move))
                break;

            Undo u_pv;
            if (!make_move(b_pv, pv_move, u_pv)) break;

            if (!pv_line.empty()) pv_line += " ";
            pv_line += move_to_uci(pv_move);

            if (pv_rep_len < MAX_PLY * 2) pv_rep_stack[pv_rep_len++] = b_pv.hash;
            else {
                std::memmove(pv_rep_stack, pv_rep_stack + 1, sizeof(U64) * (MAX_PLY * 2 - 1));
                pv_rep_stack[MAX_PLY * 2 - 1] = b_pv.hash;
            }

            if (const TTEntry* e = tt().probe(b_pv.hash)) pv_move = e->best;
            else break;
        }

        std::string score_str;
        if (std::abs(final_best_score) > MATE_SCORE - MAX_PLY) {
            int mate_dist = MATE_SCORE - std::abs(final_best_score);
            int mate_moves = (mate_dist + 1) / 2;
            if (final_best_score < 0) mate_moves = -mate_moves;
            score_str = "mate " + std::to_string(mate_moves);
        } else {
            score_str = "cp " + std::to_string(final_best_score);
        }

        if (!silent) {
            std::cout << "info depth " << depth
                      << " score " << score_str
                      << " time " << ms
                      << " nodes " << nodes
                      << " nps " << nps
                      << " pv " << pv_line << "\n";
            std::cout.flush();

            if (on_iter && !search_stopped())
                on_iter(depth, final_best_move, final_best_score, nodes, ms);
        }

        if (search_stopped()) break;
    }

    save_history();
    return {final_best_move, MOVE_NONE, final_best_score, completed_depth, searched_nodes()};
}

SearchResult search_nodes(Board &b, U64 max_nodes,
                          const U64 *rep_init, int rep_init_len,
                          const std::vector<Move> &search_moves,
                          bool silent,
                          int root_bias) {
    bool previous_local_search = local_node_limited_search;
    bool previous_local_stop = local_stop;
    U64 previous_local_count = local_node_count;
    U64 previous_local_limit = local_node_limit;

    local_node_limited_search = true;
    local_stop = false;
    local_node_count = 0;
    local_node_limit = max_nodes;

    SearchResult result = search(b, MAX_PLY - 1,
                                 rep_init, rep_init_len,
                                 search_moves,
                                 nullptr, silent, root_bias);

    local_node_limited_search = previous_local_search;
    local_stop = previous_local_stop;
    local_node_count = previous_local_count;
    local_node_limit = previous_local_limit;
    return result;
}

int qsearch_score(Board &b) {
    StackInfo st[MAX_PLY + 5]{};
    StackInfo *ss = st + 2;
    ss->acc.refresh(b);
    return qsearch(b, -INF, INF, Tune::qsearch_start_depth, 0, ss);
}

} // namespace SHAYVERI
