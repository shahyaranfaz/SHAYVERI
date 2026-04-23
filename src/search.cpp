#include "search.h"
#include "attacks.h"
#include "evaluate.h"
#include "make.h"
#include "move_gen.h"

#include <climits>

static constexpr int INF = 1000000;

static int negamax(Board &b, int depth, int alpha, int beta) {
    if (depth == 0) return evaluate(b);

    MoveList moves = generate_legal_moves(b);

    if (moves.count == 0) {
        Square ksq = king_square(b, b.side_to_move);
        if (is_square_attacked(b, ksq, flip(b.side_to_move)))
            return -INF + 1; // checkmate
        return 0; // stalemate
    }

    for (int i = 0; i < moves.count; ++i) {
        Undo u;
        make_move(b, moves.moves[i], u);
        int score = -negamax(b, depth - 1, -beta, -alpha);
        unmake_move(b, moves.moves[i], u);

        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

SearchResult search(Board& b, int max_depth) {
    SearchResult result;

    for (int depth = 1; depth <= max_depth; ++depth) {
        int alpha = -INF;
        int beta = INF;
        int best_score = -INF;
        Move best_move = MOVE_NONE;

        MoveList moves = generate_legal_moves(b);
        for (int i = 0; i < moves.count; ++i) {
            Undo u;
            make_move(b, moves.moves[i], u);
            int score = -negamax(b, depth - 1, -beta, -alpha);
            unmake_move(b, moves.moves[i], u);

            if (score > best_score) {
                best_score = score;
                best_move = moves.moves[i];
            }
            if (score > alpha) alpha = score;
        }

        result.best_move = best_move;
        result.score = best_score;
        result.depth = depth;
    }
    return result;
}