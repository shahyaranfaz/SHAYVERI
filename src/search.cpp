#include "search.h"
#include "tune.h"

#include "attacks.h"
#include "evaluate.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "see.h"
#include "tt.h"
#include "zobrist.h"

#include <atomic>
#include <algorithm>
#include <chrono>
#include <climits>
#include <iostream>
#include <cstring>
#include <cmath>
#include <memory>

namespace SHAYVERI {

std::atomic<bool> g_stop = false;
std::atomic<U64> node_count = 0;

using namespace Tune;

// Precomputed LMR table
static int LMR_TABLE[64][256];

struct InitLMR {
    InitLMR() {
        for (int d = 0; d < 64; ++d) {
            for (int moves = 0; moves < 256; ++moves) {
                if (d < 2 || moves < 2) LMR_TABLE[d][moves] = 0;
                else LMR_TABLE[d][moves] = static_cast<int>(0.75 + std::log((double)d) * std::log((double)moves) / 2.25);
            }
        }
    }
} init_lmr;

struct ScoredMove {
    Move m;
    int score;
};

// Tracks state across plies for histories and extensions
struct StackInfo {
    Move move;
    Piece piece;
    Move excluded_move;
};

struct SearchHeuristics {
    Move killers[MAX_PLY][2];
    int history[2][64][64]; // [side_to_move][from][to]

    // Continuation Histories — indexed by PieceType (1=PAWN..6=KING) to keep tables small
    int counter_move_history[7][64][7][64]; // [prev_pt][prev_to][curr_pt][curr_to]
    int follow_up_history[7][64][7][64];    // [prev2_pt][prev2_to][curr_pt][curr_to]

    SearchHeuristics() {
        std::memset(killers, 0, sizeof(killers));
        std::memset(history, 0, sizeof(history));
        std::memset(counter_move_history, 0, sizeof(counter_move_history));
        std::memset(follow_up_history, 0, sizeof(follow_up_history));
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

std::string move_to_uci(Move m) {
    auto sq_to_str = [](Square s) -> std::string {
        std::string r;
        r += char('a' + get_file(s));
        r += char('1' + get_rank(s));
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

Move uci_to_move(Board& b, const std::string& uci) {
    if (uci.size() < 4) return MOVE_NONE;
    File ff = File(uci[0] - 'a');
    Rank fr = Rank(uci[1] - '1');
    File tf = File(uci[2] - 'a');
    Rank tr = Rank(uci[3] - '1');
    Square from = make_square(ff, fr);
    Square to = make_square(tf, tr);

    PieceType promo = NONE_PTYPE;
    if (uci.size() == 5) {
        switch (uci[4]) {
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

static inline bool is_endgame(const Board &b) {
    return !(b.bit_boards[WQ] | b.bit_boards[BQ]);
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
        if (see_val >= 0) return 1000000 + (see_val * 100) + cap;
        return 700000 + (see_val * 100) + cap;
    }

    if (ply >= 0 && ply < MAX_PLY) {
        if (m == H.killers[ply][0]) return 900000;
        if (m == H.killers[ply][1]) return 890000;
    }

    int pt  = static_cast<int>(get_type(b.get_piece(move_from(m))));
    int to  = static_cast<int>(move_to(m)) & 63;
    int stm = static_cast<int>(b.side_to_move) & 1;

    int history_score = H.history[stm][int(move_from(m))][to] * Tune::main_history_weight;

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

// Step by 2 (same side to move), limit search to last half_move plies.
static inline bool is_repetition(U64 key, const U64* rep_stack, int rep_len, int half_move) {
    int limit = rep_len - half_move;
    if (limit < 0) limit = 0;
    for (int i = rep_len - 2; i >= limit; i -= 2)
        if (rep_stack[i] == key) return true;
    return false;
}

static int qsearch(Board &b, int alpha, int beta, int depth, int ply) {
    if (g_stop) return 0;
    node_count.fetch_add(1, std::memory_order_relaxed);

    Square ksq    = king_square(b, b.side_to_move);
    bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));

    int stand_pat = 0;
    if (!in_check) {
        stand_pat = evaluate(b);
        if (stand_pat >= beta) return stand_pat;
        if (stand_pat > alpha) alpha = stand_pat;
        if (depth <= 0) return alpha;
    } else {
        if (depth < -6) return evaluate(b);
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

            if (stand_pat + victim_val + 150 < alpha)
                continue;
        }

        Undo u;
        if (!make_move(b, m, u)) continue;
        legal_count++;

        int score = -qsearch(b, -beta, -alpha, depth - 1, ply + 1);
        unmake_move(b, m, u);

        if (g_stop) return 0;

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    if (in_check && legal_count == 0) return -MATE_SCORE + ply;

    return alpha;
}

static int negamax(Board &b, int depth, int alpha, int beta, int ply,
                   SearchHeuristics &H, U64 *rep_stack, int rep_len, bool pv_node, StackInfo* ss) {
    if (g_stop) return 0;
    node_count.fetch_add(1, std::memory_order_relaxed);

    const U64 key = b.hash;

    // Basic Draw Detection
    if (b.half_move >= 100) return 0;
    if (rep_len > 1 && is_repetition(key, rep_stack, rep_len - 1, b.half_move)) return 0;

    const int original_alpha = alpha;
    Move tt_move = MOVE_NONE;
    int tt_score = 0;
    int tt_depth = -1;
    int tt_bound = -1; // -1 represents "No entry found"

    if (const TTEntry *e = TT.probe(key)) {
        tt_move  = e->best;
        tt_score = e->score;
        tt_depth = e->depth;
        tt_bound = e->flag;

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

    if (depth <= 0) return qsearch(b, alpha, beta, 8, ply);

    Square ksq    = king_square(b, b.side_to_move);
    bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));
    if (in_check && ply < MAX_PLY - 1) depth++;

    const int static_eval = evaluate(b);

    // Reverse Futility Pruning
    if (!pv_node && !in_check && depth <= 5 && ss->excluded_move == MOVE_NONE) {
        if (static_eval - (Tune::rfp_margin_mult * depth) >= beta)
            return static_eval;
    }

    // Null Move Pruning
    if (!pv_node && !in_check && depth >= 3 && static_eval >= beta && !is_endgame(b) && ss->excluded_move == MOVE_NONE) {
        const int R = 3 + depth / 4;
        Undo u;
        make_null_move(b, u);
        ss->move = MOVE_NONE;
        ss->piece = NONE_PIECE;
        int score = -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, H, rep_stack, rep_len, false, ss + 1);
        unmake_null_move(b, u);

        if (g_stop) return 0;
        if (score >= beta) return score;
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
            if (is_quiet && depth <= 4 && static_eval + Tune::fp_base + Tune::fp_mult * depth <= alpha) continue;
        }

        Piece moved_piece = b.get_piece(move_from(m));
        Undo u;
        if (!make_move(b, m, u)) continue;

        legal_count++;
        if (is_quiet) quiets_played[quiet_count++] = m;
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
            if (depth >= 3 && is_quiet && m != tt_move && m != H.killers[ply][0]) {
                reduction = LMR_TABLE[std::min(depth, 63)][std::min(legal_count, 255)];
                if (!pv_node) reduction++;
                reduction = std::clamp(reduction, 0, depth - 2);
            }

            score = -negamax(b, depth - 1 - reduction + d_ext, -alpha - 1, -alpha, ply + 1, H, rep_stack, rep_len + 1, false, ss + 1);
            if (score > alpha && reduction > 0)
                score = -negamax(b, depth - 1 + d_ext, -alpha - 1, -alpha, ply + 1, H, rep_stack, rep_len + 1, false, ss + 1);
            if (pv_node && score > alpha && score < beta)
                score = -negamax(b, depth - 1 + d_ext, -beta, -alpha, ply + 1, H, rep_stack, rep_len + 1, true, ss + 1);
        }

        unmake_move(b, m, u);
        if (g_stop) return 0;

        if (score >= beta) {
            if (ss->excluded_move == MOVE_NONE) {
                int tt_s = (score > MATE_SCORE - MAX_PLY) ? score + ply : (score < -MATE_SCORE + MAX_PLY ? score - ply : score);
                TT.store(key, depth, tt_s, TT_LOWER, m);
            }
            if (is_quiet) {
                H.store_killer(ply, m);
                int bonus = std::min(Tune::history_bonus_limit, Tune::history_bonus_mult * depth - Tune::history_bonus_sub);

                auto update_all = [&](Move move, int pt_idx, int bns) {
                    int f = move_from(move);
                    int t = move_to(move);
                    H.update_history(H.history[int(b.side_to_move)][f][t], bns);

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
                update_all(m, moved_pc_idx, bonus);
                for (int j = 0; j < quiet_count - 1; ++j) {
                    int q_pc_idx = static_cast<int>(get_type(b.get_piece(move_from(quiets_played[j]))));
                    update_all(quiets_played[j], q_pc_idx, -bonus);
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
        int tt_s = (alpha > MATE_SCORE - MAX_PLY) ? alpha + ply : (alpha < -MATE_SCORE + MAX_PLY ? alpha - ply : alpha);
        TT.store(key, depth, tt_s, (alpha <= original_alpha) ? TT_UPPER : TT_EXACT, best_move);
    }

    return alpha;
}

SearchResult search(Board &b, int max_depth, const U64 *rep_init, int rep_init_len,
                    const std::vector<Move> &search_moves, IterCallback on_iter,
                    bool silent) {

    auto H_ptr = std::make_unique<SearchHeuristics>();
    SearchHeuristics &H = *H_ptr;

    // Stack setup to handle history states (allows up to ss[-2] safely at root)
    StackInfo st[MAX_PLY + 5];
    std::memset(st, 0, sizeof(st));
    StackInfo* ss = st + 2;

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
        int delta = 25;

        if (depth >= 4 && std::abs(final_best_score) < MATE_SCORE - MAX_PLY) {
            window_alpha = std::max(-INF, final_best_score - delta);
            window_beta  = std::min(INF, final_best_score + delta);
        }

        // Generate and order root moves once per depth (outside aspiration retry loop).
        MoveList pseudo = generate_pseudo_legal_moves(b);
        Move tt_root = MOVE_NONE;
        if (const TTEntry* e = TT.probe(b.hash)) tt_root = e->best;
        ScoredMove ordered[256];
        for (int i = 0; i < pseudo.count; ++i) {
            Move m = pseudo.moves[i];
            int  s = order_score(b, m, 0, H, ss);
            if (tt_root != MOVE_NONE && m == tt_root) s += 2000000;
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

                if (g_stop) break;

                if (score > best_score_this_depth) {
                    best_score_this_depth = score;
                    best_move_this_depth  = m;
                }

                if (score > current_alpha) current_alpha = score;
            }

            if (g_stop) break;

            if (legal_root_count == 0) {
                Square ksq = king_square(b, b.side_to_move);
                bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));
                return {MOVE_NONE, MOVE_NONE, in_check ? -MATE_SCORE : 0, 0, 0};
            }

            if (best_score_this_depth <= window_alpha && window_alpha > -INF) {
                window_alpha = std::max(-INF, window_alpha - delta);
                delta *= 2;
                continue;
            } else if (best_score_this_depth >= window_beta && window_beta < INF) {
                window_beta = std::min(INF, window_beta + delta);
                delta *= 2;
                continue;
            }

            final_best_score = best_score_this_depth;
            final_best_move  = best_move_this_depth;
            completed_depth  = depth;
            break;
        }

        if (g_stop) break;

        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (ms == 0) ms = 1;
        U64 nodes = node_count.load(std::memory_order_relaxed);
        U64 nps = (nodes * 1000ULL) / ms;

        std::string pv_line = move_to_uci(final_best_move);
        Board b_pv = b;
        Undo u_pv;
        if (make_move(b_pv, final_best_move, u_pv)) {
            for (int i = 0; i < depth - 1; ++i) {
                if (const TTEntry* e = TT.probe(b_pv.hash)) {
                    if (e->best != MOVE_NONE) {
                        pv_line += " " + move_to_uci(e->best);
                        if (!make_move(b_pv, e->best, u_pv)) break;
                    } else break;
                } else break;
            }
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

            if (on_iter && !g_stop)
                on_iter(depth, final_best_move, final_best_score, nodes, ms);
        }

        if (g_stop) break;
    }

    return {final_best_move, MOVE_NONE, final_best_score, completed_depth, node_count.load(std::memory_order_relaxed)};
}

} // namespace SHAYVERI
