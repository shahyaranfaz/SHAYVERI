#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"
#include "move_io.h"
#include "tt.h"

#include <atomic>
#include <functional>
#include <vector>

namespace SHAYVERI {

struct SearchContext {
    explicit SearchContext(TranspositionTable &transposition_table)
        : table(transposition_table) {}

    TranspositionTable &table;
    std::atomic<bool> stop{false};
    std::atomic<U64> nodes{0};
    std::atomic<U64> node_limit{0};
};

extern SearchContext DefaultSearchContext;

// depth, best move, score, total nodes, elapsed ms, best-root-move node share
using IterCallback = std::function<void(int, Move, int, U64, I64, double)>;

struct SearchResult {
    Move best_move  = MOVE_NONE;
    Move ponder_move = MOVE_NONE;
    int  score      = 0;
    int  depth      = 0;
    U64  nodes      = 0;
};

namespace SearchDetail {

struct SingularSearchDecision {
    int  extension = 0;
    bool multicut  = false;
};

SingularSearchDecision classify_singular_search(
    int singular_score, int singular_beta, int beta, int tt_score, bool cut_node);

} // namespace SearchDetail

SearchResult search(SearchContext &context,
                    Board &b, int max_depth,
                    const U64 *rep_init, int rep_init_len,
                    const std::vector<Move> &search_moves,
                    IterCallback on_iter = nullptr,
                    bool silent = false,
                    int root_bias = 0);

SearchResult search(Board &b, int max_depth,
                    const U64 *rep_init, int rep_init_len,
                    const std::vector<Move> &search_moves,
                    IterCallback on_iter = nullptr,
                    bool silent = false,
                    int root_bias = 0);

SearchResult search_nodes(SearchContext &context,
                          Board &b, U64 max_nodes,
                          const U64 *rep_init, int rep_init_len,
                          const std::vector<Move> &search_moves,
                          bool silent = true,
                          int root_bias = 0);

SearchResult search_nodes(Board &b, U64 max_nodes,
                          const U64 *rep_init, int rep_init_len,
                          const std::vector<Move> &search_moves,
                          bool silent = true,
                          int root_bias = 0);

int qsearch_score(SearchContext &context, Board &b);
int qsearch_score(Board &b);
void clear_search_histories();

} // namespace SHAYVERI

#endif // SEARCH_H
