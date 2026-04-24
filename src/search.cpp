#include "search.h"

#include "attacks.h"
#include "evaluate.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "tt.h"

#include <algorithm>
#include <climits>
#include <cstring>

static constexpr int MAX_PLY = 128;
static constexpr int INF = 1000000;
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
static inline bool is_ep_capture(const Board& b, Move m) {
    if (b.en_passant == SQ_NONE) return false;
    if (move_to(m) != b.en_passant) return false;

    Square from = move_from(m);
    Square to   = move_to(m);

    Piece p = b.get_piece(from);
    if (p != WP && p != BP) return false;

    int df = int(get_file(to)) - int(get_file(from));
    if (df != 1 && df != -1) return false;

    int dr = int(get_rank(to)) - int(get_rank(from));
    if (p == WP && dr != 1) return false;
    if (p == BP && dr != -1) return false;

    return true;
}

static inline int capture_order_score(const Board& b, Move m) {
    if (move_promo(m) != NONE_PTYPE)
        return 1000000;

    Square from = move_from(m);
    Square to   = move_to(m);

    Piece attacker_piece = b.get_piece(from);
    PieceType attacker   = get_type(attacker_piece);
    PieceType victim = NONE_PTYPE;

    if (is_ep_capture(b, m)) {
        victim = PAWN;
    } else {
        Piece victim_piece = b.get_piece(to);
        if (victim_piece == NONE_PIECE) return 0; // quiet
        victim = get_type(victim_piece);
    }

    return 100000 + (PTYPE_VALUES[victim] - PTYPE_VALUES[attacker]);
}

static inline bool is_capture_or_promo(const Board& b, Move m) {
    if (move_promo(m) != NONE_PTYPE) return true;
    if (is_ep_capture(b, m)) return true;
    return b.get_piece(move_to(m)) != NONE_PIECE;
}

// captures first (MVV/LVA), then killer, then history
static inline int order_score(const Board& b, Move m, int ply, const SearchHeuristics& H) {
    int cap = capture_order_score(b, m);
    if (cap != 0) return 1000000 + cap;

    if (ply >= 0 && ply < MAX_PLY) {
        if (m == H.killers[ply][0]) return 900000;
        if (m == H.killers[ply][1]) return 890000;
    }

    Square from = move_from(m);
    Square to   = move_to(m);
    return H.history[int(from)][int(to)];
}

static inline bool is_repetition(U64 key, const U64* rep_stack, int rep_len) {
    for (int i = rep_len - 1; i >= 0; --i)
        if (rep_stack[i] == key) return true;
    return false;
}

static int qsearch(Board &b, int alpha, int beta, int depth) {
    if (g_stop) return 0;
    node_count++;

    int stand_pat = evaluate(b);
    if (stand_pat >= beta) return stand_pat;
    if (stand_pat > alpha) alpha = stand_pat;

    if (depth <= 0) return alpha;

    MoveList moves = generate_pseudo_legal_moves(b);

    ScoredMove noisy[256];
    int noisy_count = 0;

    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        int s = capture_order_score(b, m);
        if (s == 0) continue;
        noisy[noisy_count++] = {m, s};
    }

    std::sort(noisy, noisy + noisy_count, [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    for (int i = 0; i < noisy_count; ++i) {
        Move m = noisy[i].m;

        Undo u;
        if (!make_move(b, m, u)) continue;

        int score = -qsearch(b, -beta, -alpha, depth - 1);

        unmake_move(b, m, u);

        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }

    return alpha;
}

static int negamax(Board &b, int depth, int alpha, int beta, int ply, SearchHeuristics &H, U64 *rep_stack, int rep_len) {
    if (g_stop) return 0;
    node_count++;

    U64 key = b.hash;

    if (rep_len > 1 && is_repetition(key, rep_stack, rep_len - 1)) return 0;

    int original_alpha = alpha;
    Move tt_move = MOVE_NONE;
    if (const TTEntry *curr = TT.probe(key)) {
        tt_move = curr->best;
        if (curr->depth >= depth) {
            int s = curr->score;
            if (curr->flag == TT_EXACT) return s;
            if (curr->flag == TT_LOWER) alpha = std::max(alpha, s);
            else if (curr->flag == TT_UPPER) beta = std::min(beta, s);
            if (alpha >= beta) return s;
        }
    }

    if (depth == 0) return qsearch(b, alpha, beta, 8);

    MoveList moves = generate_legal_moves(b);

    if (moves.count == 0) {
        Square ksq = king_square(b, b.side_to_move);
        if (is_square_attacked(b, ksq, flip(b.side_to_move))) return -MATE_SCORE;
        return 0;
    }

    Move best_move = MOVE_NONE;

    ScoredMove ordered[256];
    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        int s = order_score(b, m, ply, H);
        if (tt_move != MOVE_NONE && m == tt_move) s += 2000000; // try TT move first
        ordered[i] = {m, s};
    }

    std::sort(ordered, ordered + moves.count, [](const ScoredMove& a, const ScoredMove& b) {
        return a.score > b.score;
    });

    for (int i = 0; i < moves.count; ++i) {
        Move m = ordered[i].m;

        Undo u;
        if (!make_move(b, m, u)) continue;
        rep_stack[rep_len] = b.hash;
        int score = -negamax(b, depth - 1, -beta, -alpha, ply + 1, H, rep_stack, rep_len + 1);
        unmake_move(b, m, u);

        if (score >= beta) {
            // store cutoff as LOWER bound
            TT.store(key, depth, score, TT_LOWER, m);
            if (!is_capture_or_promo(b, m)) {
                H.store_killer(ply, m);
                H.add_history(m, depth);
            }
            return score;
        }

        if (score > alpha) {
            alpha = score;
            best_move = m;
        }
    }
    // store final node result
    TTFlag flag = (alpha <= original_alpha) ? TT_UPPER : TT_EXACT;
    TT.store(key, depth, alpha, flag, best_move);
    return alpha;
}

SearchResult search(Board &b, int max_depth, const U64 *rep_init, int rep_init_len) {
    SearchResult result;
    SearchHeuristics H;
    node_count = 0;
    g_stop = false;

    U64 rep_stack[MAX_PLY];
    int rep_len = 0;

    if (rep_init && rep_init_len > 0) {
        rep_len = std::min(rep_init_len, MAX_PLY);
        for (int i = 0; i < rep_len; ++i)
            rep_stack[i] = rep_init[i];
    }

    if (rep_len == 0 || rep_stack[rep_len - 1] != b.hash) {
        if (rep_len < MAX_PLY)
            rep_stack[rep_len++] = b.hash;
        else
            rep_stack[MAX_PLY - 1] = b.hash;
    }

    for (int depth = 1; depth <= max_depth; ++depth) {
        int alpha = -INF;
        int beta  = INF;
        int best_score = -INF;
        Move best_move = MOVE_NONE;

        MoveList moves = generate_legal_moves(b);
        Move tt_root = MOVE_NONE;
        if (const TTEntry* e = TT.probe(b.hash)) tt_root = e->best;

        // Root move ordering also uses the same scheme.
        ScoredMove ordered[256];
        for (int i = 0; i < moves.count; ++i) {
            Move m = moves.moves[i];
            int s = order_score(b, m, 0, H);
            if (tt_root != MOVE_NONE && m == tt_root) s += 2000000;
            ordered[i] = {m, s};
        }
        std::sort(ordered, ordered + moves.count, [](const ScoredMove& a, const ScoredMove& b) {
            return a.score > b.score;
        });

        for (int i = 0; i < moves.count; ++i) {
            Move m = ordered[i].m;

            Undo u;
            if (!make_move(b, m, u)) continue;
            rep_stack[rep_len] = b.hash;
            int score = -negamax(b, depth - 1, -beta, -alpha, 1, H, rep_stack, rep_len + 1);
            unmake_move(b, m, u);

            if (score > best_score) {
                best_score = score;
                best_move = m;
            }
            if (score > alpha) alpha = score;
        }
        if (g_stop) break;

        result.best_move = best_move;
        result.score = best_score;
        result.depth = depth;
        result.nodes = node_count;
    }
    return result;
}
