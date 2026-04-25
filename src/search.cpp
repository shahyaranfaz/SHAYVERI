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
#include <climits>
#include <cstring>

static constexpr int MAX_PLY    = 128;
static constexpr int INF        = 1000000;
static constexpr int MATE_SCORE = 900000;
static constexpr int PTYPE_VALUES[7] = {0, 100, 320, 330, 500, 900, 0};

std::atomic<bool> g_stop = false;
static U64 node_count = 0;

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
        int bonus = depth * depth;
        history[int(from)][int(to)] += bonus;
        if (history[int(from)][int(to)] > 1000000)
            history[int(from)][int(to)] /= 2;
    }
};

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

static inline int order_score(const Board& b, Move m, int ply, const SearchHeuristics& H) {
    int cap = capture_order_score(b, m);
    if (cap != 0)
        return see_ge_zero(b, m) ? 1000000 + cap : 800000 + cap;

    if (ply >= 0 && ply < MAX_PLY) {
        if (m == H.killers[ply][0]) return 900000;
        if (m == H.killers[ply][1]) return 890000;
    }

    return H.history[int(move_from(m))][int(move_to(m))];
}

static inline bool is_repetition(U64 key, const U64* rep_stack, int rep_len) {
    for (int i = rep_len - 1; i >= 0; --i)
        if (rep_stack[i] == key) return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Quiescence search
//
// When in check we cannot use stand-pat — the side to move has no "do nothing"
// option.  We therefore search ALL moves as evasions and skip every pruning
// heuristic that assumes a quiet stand-pat baseline (SEE filter, delta pruning).
// A hard depth floor stops runaway evasion chains in artificial positions.
// ─────────────────────────────────────────────────────────────────────────────
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
        // Hard floor: prevent unbounded evasion chains
        if (depth < -6) return evaluate(b);
    }

    MoveList moves = generate_pseudo_legal_moves(b);

    ScoredMove noisy[256];
    int noisy_count = 0;

    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        int s  = capture_order_score(b, m);
        // In check: include quiet evasions; out of check: captures/promos only
        if (!in_check && s == 0) continue;
        noisy[noisy_count++] = {m, s};
    }

    std::sort(noisy, noisy + noisy_count,
        [](const ScoredMove& a, const ScoredMove& b) {
            return a.score > b.score;
        });

    int legal_count = 0;

    for (int i = 0; i < noisy_count; ++i) {
        Move m = noisy[i].m;

        if (!in_check) {
            // SEE filter: skip losing captures (not promos, not ep)
            if (!is_promo(m) && b.get_piece(move_to(m)) != NONE_PIECE && !see_ge_zero(b, m))
                continue;

            // Delta pruning
            Piece vp = b.get_piece(move_to(m));
            int victim_val = 0;
            if (vp != NONE_PIECE)
                victim_val = PTYPE_VALUES[get_type(vp)];
            else if (is_ep_capture(b, m))
                victim_val = PTYPE_VALUES[PAWN];
            if (move_promo(m) != NONE_PTYPE)
                victim_val += PTYPE_VALUES[move_promo(m)];
            if (stand_pat + victim_val + 150 < alpha)
                continue;
        }

        Undo u;
        if (!make_move(b, m, u)) continue;
        legal_count++;

        int score = -qsearch(b, -beta, -alpha, depth - 1, ply + 1);
        unmake_move(b, m, u);

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    // In check with no legal moves → checkmate
    if (in_check && legal_count == 0) return -MATE_SCORE + ply;

    return alpha;
}

// ─────────────────────────────────────────────────────────────────────────────
// Negamax
//
// Uses generate_pseudo_legal_moves; make_move enforces legality.
// Terminal detection (checkmate / stalemate) happens after the move loop once
// legal_count is known — no need for a separate generate_legal_moves pass.
// Futility and LMR conditions use legal_count, not the sorted-candidate index.
// ─────────────────────────────────────────────────────────────────────────────
static int negamax(Board &b, int depth, int alpha, int beta, int ply,
                   SearchHeuristics &H, U64 *rep_stack, int rep_len) {
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

            if (e->flag == TT_EXACT) return s;
            if (e->flag == TT_LOWER) alpha = std::max(alpha, s);
            else if (e->flag == TT_UPPER) beta = std::min(beta, s);
            if (alpha >= beta) return s;
        }
    }

    if (depth == 0) return qsearch(b, alpha, beta, 8, ply);

    Square ksq    = king_square(b, b.side_to_move);
    bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));

    if (in_check && ply < MAX_PLY - 1) depth++;

    const int static_eval = evaluate(b);

    // ── 1. Reverse futility pruning ──────────────────────────────────────────
    if (!in_check && depth <= 3 && static_eval - 150 * depth >= beta)
        return static_eval;

    // ── 2. Null move pruning ─────────────────────────────────────────────────
    if (!in_check && depth >= 3 && static_eval >= beta && !is_endgame(b)) {
        const int R = 2 + depth / 4;
        Undo u;
        make_null_move(b, u);
        int score = -negamax(b, depth - 1 - R, -beta, -beta + 1,
                             ply + 1, H, rep_stack, rep_len);
        unmake_null_move(b, u);
        if (score >= beta) return score;
    }

    const bool do_futility = (depth <= 2 && !in_check &&
                               alpha > -MATE_SCORE + MAX_PLY);

    // ── Sort pseudo-legal moves ──────────────────────────────────────────────
    MoveList pseudo = generate_pseudo_legal_moves(b);

    ScoredMove ordered[256];
    for (int i = 0; i < pseudo.count; ++i) {
        Move m = pseudo.moves[i];
        int s  = order_score(b, m, ply, H);
        if (tt_move != MOVE_NONE && m == tt_move) s += 2000000;
        ordered[i] = {m, s};
    }
    std::sort(ordered, ordered + pseudo.count,
        [](const ScoredMove& a, const ScoredMove& b) {
            return a.score > b.score;
        });

    Move best_move  = MOVE_NONE;
    int  legal_count = 0;

    for (int i = 0; i < pseudo.count; ++i) {
        Move m = ordered[i].m;
        bool is_quiet = !is_capture_or_promo(b, m);

        // ── 3. Futility pruning ──────────────────────────────────────────────
        // Skip the very first legal move so we always have a score to return.
        // legal_count (not i) reflects how many moves have actually been made.
        if (do_futility && is_quiet && legal_count >= 1 &&
            m != tt_move &&
            m != H.killers[ply][0] &&
            m != H.killers[ply][1]) {
            if (static_eval + 100 + 150 * depth <= alpha)
                continue;
        }

        Undo u;
        if (!make_move(b, m, u)) continue; // legality check
        legal_count++;
        rep_stack[rep_len] = b.hash;

        int score;

        // ── 4. Late move reductions ──────────────────────────────────────────
        // legal_count (not i) is the correct gate: we want at least 3 legal
        // moves to have been searched before reducing.
        if (depth >= 3 && legal_count > 3 && is_quiet &&
            m != tt_move &&
            m != H.killers[ply][0] &&
            m != H.killers[ply][1]) {

            int reduction = 1 + (depth / 4) + (legal_count / 6);
            if (reduction > depth - 2) reduction = depth - 2;

            score = -negamax(b, depth - 1 - reduction,
                             -alpha - 1, -alpha,
                             ply + 1, H, rep_stack, rep_len + 1);
            if (score > alpha)
                score = -negamax(b, depth - 1, -beta, -alpha,
                                 ply + 1, H, rep_stack, rep_len + 1);
        } else {
            score = -negamax(b, depth - 1, -beta, -alpha,
                             ply + 1, H, rep_stack, rep_len + 1);
        }

        unmake_move(b, m, u);

        if (score >= beta) {
            int ss = score;
            if (ss >  MATE_SCORE - MAX_PLY) ss += ply;
            if (ss < -MATE_SCORE + MAX_PLY) ss -= ply;
            TT.store(key, depth, ss, TT_LOWER, m);

            if (!is_capture_or_promo(b, m)) {
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

    // ── Terminal detection ───────────────────────────────────────────────────
    // Done here rather than up front: avoids the cost of generate_legal_moves.
    if (legal_count == 0)
        return in_check ? -MATE_SCORE + ply : 0;

    int sa = alpha;
    if (sa >  MATE_SCORE - MAX_PLY) sa += ply;
    if (sa < -MATE_SCORE + MAX_PLY) sa -= ply;
    TT.store(key, depth, sa,
             (alpha <= original_alpha) ? TT_UPPER : TT_EXACT,
             best_move);

    return alpha;
}

// ─────────────────────────────────────────────────────────────────────────────
// Root search  (iterative deepening)
// ─────────────────────────────────────────────────────────────────────────────
SearchResult search(Board &b, int max_depth, const U64 *rep_init, int rep_init_len) {
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
        for (int i = 0; i < rep_len; ++i)
            rep_stack[i] = rep_init[i];
    }
    if (rep_len == 0 || rep_stack[rep_len - 1] != b.hash) {
        if (rep_len < MAX_PLY) rep_stack[rep_len++] = b.hash;
        else                   rep_stack[MAX_PLY - 1] = b.hash;
    }

    for (int depth = 1; depth <= max_depth; ++depth) {
        int alpha = -INF, beta = INF;
        int  best_score_this_depth = -INF;
        Move best_move_this_depth  = MOVE_NONE;

        // Pseudo-legal at root — same pattern as negamax
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
        std::sort(ordered, ordered + pseudo.count,
            [](const ScoredMove& a, const ScoredMove& b) {
                return a.score > b.score;
            });

        int legal_root_count = 0;

        for (int i = 0; i < pseudo.count; ++i) {
            Move m = ordered[i].m;
            Undo u;
            if (!make_move(b, m, u)) continue;
            legal_root_count++;
            rep_stack[rep_len] = b.hash;

            int score = -negamax(b, depth - 1, -beta, -alpha,
                                 1, H, rep_stack, rep_len + 1);
            unmake_move(b, m, u);

            if (g_stop) break;

            if (score > best_score_this_depth) {
                best_score_this_depth = score;
                best_move_this_depth  = m;
            }
            if (score > alpha) alpha = score;
        }

        // Terminal position (checkmate or stalemate at root)
        if (legal_root_count == 0) {
            Square ksq    = king_square(b, b.side_to_move);
            bool in_check = is_square_attacked(b, ksq, flip(b.side_to_move));
            return {MOVE_NONE, in_check ? -MATE_SCORE : 0, 0, 0};
        }

        if (g_stop) break;

        final_best_score = best_score_this_depth;
        final_best_move  = best_move_this_depth;
        completed_depth  = depth;
    }

    return {final_best_move, final_best_score, completed_depth, node_count};
}