#include "search.h"

#include "attacks.h"
#include "evaluate.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "see.h"
#include "tt.h"
#include "zobrist.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <iostream>
#include <cstring>
#include <cmath>

static constexpr int MAX_PLY    = 128;
static constexpr int INF        = 1000000;
static constexpr int MATE_SCORE = 900000;
static constexpr int PTYPE_VALUES[7] = {0, 100, 320, 330, 500, 900, 0};

std::atomic<bool> g_stop = false;
static U64 node_count = 0;

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

struct SearchHeuristics {
    Move killers[MAX_PLY][2];
    int history[64][64];

    SearchHeuristics() {
        for (int i = 0; i < MAX_PLY; ++i) {
            killers[i][0] = MOVE_NONE;
            killers[i][1] = MOVE_NONE;
        }
        std::memset(history, 0, sizeof(history));
    }

    inline void store_killer(int ply, Move m) {
        if (ply < 0 || ply >= MAX_PLY) return;
        if (killers[ply][0] == m) return;
        killers[ply][1] = killers[ply][0];
        killers[ply][0] = m;
    }

    inline void add_history(Move m, int depth) {
        Square from = move_from(m);
        Square to   = move_to(m);
        int bonus = depth * depth; // Quadratic history bonus
        history[int(from)][int(to)] += bonus;
        if (history[int(from)][int(to)] > 1000000) {
            for (int r = 0; r < 64; ++r)
                for (int c = 0; c < 64; ++c)
                    history[r][c] /= 2;
        }
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
            case ROOK: pc = 'r'; break;
            case QUEEN: pc = 'q'; break;
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

static inline bool is_ep_capture(const Board& b, Move m) {
    if (b.en_passant == SQ_NONE) return false;
    if (move_to(m) != b.en_passant) return false;
    Piece p = b.get_piece(move_from(m));
    if (p != WP && p != BP) return false;
    int df = int(get_file(move_to(m))) - int(get_file(move_from(m)));
    if (df != 1 && df != -1) return false;
    int dr = int(get_rank(move_to(m))) - int(get_rank(move_from(m)));
    if (p == WP && dr != 1)  return false;
    if (p == BP && dr != -1) return false;
    return true;
}

static inline int capture_order_score(const Board& b, Move m) {
    if (move_promo(m) != NONE_PTYPE)
        return 1000000;

    PieceType victim = NONE_PTYPE;
    if (is_ep_capture(b, m)) {
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
    if (is_ep_capture(b, m)) return true;
    return b.get_piece(move_to(m)) != NONE_PIECE;
}

// Overhauled Move Ordering
static inline int order_score(const Board& b, Move m, int ply, const SearchHeuristics& H) {
    int cap = capture_order_score(b, m);
    if (cap != 0) {
        // Good/neutral captures vs Bad captures
        if (see_ge_zero(b, m)) return 1000000 + cap; // Favorable captures
        else return 700000 + cap;                   // Unfavorable captures
    }

    if (ply >= 0 && ply < MAX_PLY) {
        if (m == H.killers[ply][0]) return 900000;
        if (m == H.killers[ply][1]) return 890000;
    }

    // Quiet moves sorted by history score
    return H.history[int(move_from(m))][int(move_to(m))];
}

static inline bool is_repetition(U64 key, const U64* rep_stack, int rep_len) {
    for (int i = rep_len - 1; i >= 0; --i)
        if (rep_stack[i] == key) return true;
    return false;
}

// Quiescence search
static int qsearch(Board &b, int alpha, int beta, int depth, int ply) {
    if (g_stop) return 0;
    node_count++;

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

    MoveList moves = generate_pseudo_legal_moves(b);
    ScoredMove noisy[256];
    int noisy_count = 0;

    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        int s  = capture_order_score(b, m);
        if (!in_check && s == 0) continue;
        noisy[noisy_count++] = {m, s};
    }

    std::sort(noisy, noisy + noisy_count, [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });

    int legal_count = 0;

    for (int i = 0; i < noisy_count; ++i) {
        Move m = noisy[i].m;

        if (!in_check) {
            if (!is_promo(m) && b.get_piece(move_to(m)) != NONE_PIECE && !see_ge_zero(b, m))
                continue;

            Piece vp = b.get_piece(move_to(m));
            int victim_val = (vp != NONE_PIECE) ? PTYPE_VALUES[get_type(vp)] : (is_ep_capture(b, m) ? PTYPE_VALUES[PAWN] : 0);
            if (move_promo(m) != NONE_PTYPE) victim_val += PTYPE_VALUES[move_promo(m)];

            // Delta pruning
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

// Negamax with Principal Variation Search (PVS)
static int negamax(Board &b, int depth, int alpha, int beta, int ply,
                   SearchHeuristics &H, U64 *rep_stack, int rep_len, bool pv_node) {
    if (g_stop) return 0;
    node_count++;

    const U64 key = b.hash;

    if (b.half_move >= 100) return 0;
    if (rep_len > 1 && is_repetition(key, rep_stack, rep_len - 1)) return 0;

    const int original_alpha = alpha;
    Move tt_move = MOVE_NONE;

    if (const TTEntry *e = TT.probe(key)) {
        tt_move = e->best;
        if (e->depth >= depth) {
            int s = e->score;
            if (s > MATE_SCORE - MAX_PLY) s -= ply;
            else if (s < -MATE_SCORE + MAX_PLY) s += ply;

            // Only prune based on TT if it's not a PV node
            if (!pv_node) {
                if (e->flag == TT_EXACT) return s;
                if (e->flag == TT_LOWER) alpha = std::max(alpha, s);
                else if (e->flag == TT_UPPER) beta = std::min(beta, s);
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
    if (!pv_node && !in_check && depth <= 5) {
        int eval_margin = 120 * depth;
        if (static_eval - eval_margin >= beta)
            return static_eval;
    }

    // Null Move Pruning
    if (!pv_node && !in_check && depth >= 3 && static_eval >= beta && !is_endgame(b)) {
        const int R = 3 + depth / 4;
        Undo u;
        make_null_move(b, u);
        int score = -negamax(b, depth - 1 - R, -beta, -beta + 1, ply + 1, H, rep_stack, rep_len, false);
        unmake_null_move(b, u);

        if (g_stop) return 0;
        if (score >= beta) return score;
    }

    MoveList pseudo = generate_pseudo_legal_moves(b);

    ScoredMove ordered[256];
    for (int i = 0; i < pseudo.count; ++i) {
        Move m = pseudo.moves[i];
        int s  = order_score(b, m, ply, H);
        if (tt_move != MOVE_NONE && m == tt_move) s += 2000000;
        ordered[i] = {m, s};
    }
    std::sort(ordered, ordered + pseudo.count, [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });

    Move best_move  = MOVE_NONE;
    int  legal_count = 0;

    // LMP Threshold
    int lmp_threshold = 3 + 2 * depth * depth;

    for (int i = 0; i < pseudo.count; ++i) {
        Move m = ordered[i].m;
        bool is_quiet = !is_capture_or_promo(b, m);

        // Late Move Pruning (LMP)
        if (!pv_node && !in_check && is_quiet && legal_count > lmp_threshold)
            continue;

        // Futility Pruning
        if (!pv_node && !in_check && is_quiet && depth <= 4 && m != tt_move && m != H.killers[ply][0] && m != H.killers[ply][1]) {
            if (static_eval + 150 + 150 * depth <= alpha)
                continue;
        }

        Undo u;
        if (!make_move(b, m, u)) continue;
        legal_count++;
        rep_stack[rep_len] = b.hash;

        int score;

        // Principal Variation Search (PVS) & LMR
        if (legal_count == 1) {
            // First move: Full window search
            score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, H, rep_stack, rep_len + 1, pv_node);
        } else {
            // Late Move Reduction calculation
            int reduction = 0;
            if (depth >= 3 && legal_count > 1 && is_quiet && m != tt_move && m != H.killers[ply][0] && m != H.killers[ply][1]) {
                int capped_d = std::min(depth, 63);
                int capped_l = std::min(legal_count, 255);
                reduction = LMR_TABLE[capped_d][capped_l];

                if (!pv_node) reduction++; // Increase reduction for non-PV nodes
                reduction = std::min(reduction, depth - 2);
                reduction = std::max(reduction, 0);
            }

            // Null window search
            score = -negamax(b, depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, H, rep_stack, rep_len + 1, false);

            // Re-search if LMR failed high, or PVS expects a better score
            if (score > alpha && reduction > 0) {
                score = -negamax(b, depth - 1, -alpha - 1, -alpha, ply + 1, H, rep_stack, rep_len + 1, false);
            }
            if (pv_node && score > alpha && score < beta) {
                score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, H, rep_stack, rep_len + 1, true);
            }
        }

        unmake_move(b, m, u);

        if (g_stop) return 0;

        if (score >= beta) {
            int ss = score;
            if (ss >  MATE_SCORE - MAX_PLY) ss += ply;
            if (ss < -MATE_SCORE + MAX_PLY) ss -= ply;
            TT.store(key, depth, ss, TT_LOWER, m);

            if (is_quiet) {
                H.store_killer(ply, m);
                H.add_history(m, depth);
            }
            return score;
        }

        if (score > alpha) {
            alpha    = score;
            best_move = m;
        }
    }

    if (legal_count == 0) return in_check ? -MATE_SCORE + ply : 0;

    int sa = alpha;
    if (sa >  MATE_SCORE - MAX_PLY) sa += ply;
    if (sa < -MATE_SCORE + MAX_PLY) sa -= ply;
    TT.store(key, depth, sa, (alpha <= original_alpha) ? TT_UPPER : TT_EXACT, best_move);

    return alpha;
}

// Root search (Iterative deepening + Aspiration Windows)
SearchResult search(Board &b, int max_depth, const U64 *rep_init, int rep_init_len, const std::vector<Move> &search_moves) {
    node_count = 0;
    g_stop     = false;

    SearchHeuristics H;
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

    // Timer initialized BEFORE depth loops
    auto start_time = std::chrono::steady_clock::now();

    for (int depth = 1; depth <= max_depth; ++depth) {
        // Setup Aspiration Window
        int window_alpha = -INF;
        int window_beta  = INF;
        int delta = 25;

        if (depth >= 4 && std::abs(final_best_score) < MATE_SCORE - MAX_PLY) {
            window_alpha = std::max(-INF, final_best_score - delta);
            window_beta  = std::min(INF, final_best_score + delta);
        }

        while (true) {
            // Internal window state that tracking mutations during the move loop
            int current_alpha = window_alpha;
            int current_beta  = window_beta;

            int best_score_this_depth = -INF;
            Move best_move_this_depth = MOVE_NONE;

            MoveList pseudo = generate_pseudo_legal_moves(b);

            Move tt_root = MOVE_NONE;
            if (const TTEntry* e = TT.probe(b.hash)) tt_root = e->best;

            ScoredMove ordered[256];
            for (int i = 0; i < pseudo.count; ++i) {
                Move m = pseudo.moves[i];
                int  s = order_score(b, m, 0, H);
                if (tt_root != MOVE_NONE && m == tt_root) s += 2000000;
                ordered[i] = {m, s};
            }
            std::sort(ordered, ordered + pseudo.count, [](const ScoredMove& a, const ScoredMove& b) { return a.score > b.score; });

            int legal_root_count = 0;

            for (int i = 0; i < pseudo.count; ++i) {
                Move m = ordered[i].m;

                if (!search_moves.empty()) {
                    bool found = false;
                    for (Move sm : search_moves) if (sm == m) { found = true; break; }
                    if (!found) continue;
                }

                Undo u;
                if (!make_move(b, m, u)) continue;
                legal_root_count++;
                rep_stack[rep_len] = b.hash;

                int score;
                if (legal_root_count == 1) {
                    score = -negamax(b, depth - 1, -current_beta, -current_alpha, 1, H, rep_stack, rep_len + 1, true);
                } else {
                    score = -negamax(b, depth - 1, -current_alpha - 1, -current_alpha, 1, H, rep_stack, rep_len + 1, false);
                    if (score > current_alpha && score < current_beta) {
                        score = -negamax(b, depth - 1, -current_beta, -current_alpha, 1, H, rep_stack, rep_len + 1, true);
                    }
                }

                unmake_move(b, m, u);

                if (g_stop) break;

                if (score > best_score_this_depth) {
                    best_score_this_depth = score;
                    best_move_this_depth  = m;
                }

                // Track internally without corrupting the bounds check below
                if (score > current_alpha) current_alpha = score;
            }

            if (g_stop) break;

            if (legal_root_count == 0) {
                Square ksq = king_square(b, b.side_to_move);
                bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));
                return {MOVE_NONE, in_check ? -MATE_SCORE : 0, 0, 0};
            }

            // Aspiration Window Check - Comparing against preserved 'window' state
            if (best_score_this_depth <= window_alpha && window_alpha > -INF) {
                window_alpha = std::max(-INF, window_alpha - delta);
                delta *= 2; // Widening lower bound
                continue; // Re-search
            } else if (best_score_this_depth >= window_beta && window_beta < INF) {
                window_beta = std::min(INF, window_beta + delta);
                delta *= 2; // Widening upper bound
                continue; // Re-search
            }

            final_best_score = best_score_this_depth;
            final_best_move  = best_move_this_depth;
            completed_depth  = depth;
            break; // Aspiration window hit
        }

        if (g_stop) break;

        // Calculate Time & NPS
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (ms == 0) ms = 1;
        U64 nps = (node_count * 1000ULL) / ms;

        // Extract PV
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

        std::cout << "info depth " << depth
                  << " score " << score_str
                  << " time " << ms
                  << " nodes " << node_count
                  << " nps " << nps
                  << " pv " << pv_line << "\n";
        std::cout.flush();
    }

    return {final_best_move, final_best_score, completed_depth, node_count};
}