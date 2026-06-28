#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"

#include <atomic>
#include <functional>
#include <vector>

namespace SHAYVERI {

extern std::atomic<bool> g_stop;
extern std::atomic<U64>  node_count;
extern std::atomic<U64>  node_limit;

using IterCallback = std::function<void(int, Move, int, U64, I64)>;

struct SearchResult {
    Move best_move  = MOVE_NONE;
    Move ponder_move = MOVE_NONE;
    int  score      = 0;
    int  depth      = 0;
    U64  nodes      = 0;
};

std::string move_to_uci(Move m);
Move        uci_to_move(Board &b, const std::string &uci);

SearchResult search(Board &b, int max_depth,
                    const U64 *rep_init, int rep_init_len,
                    const std::vector<Move> &search_moves,
                    IterCallback on_iter = nullptr,
                    bool silent = false,
                    int root_bias = 0);

SearchResult search_nodes(Board &b, U64 max_nodes,
                          const U64 *rep_init, int rep_init_len,
                          const std::vector<Move> &search_moves,
                          bool silent = true,
                          int root_bias = 0);

int qsearch_score(Board &b);
void clear_search_histories();

} // namespace SHAYVERI

#endif // SEARCH_H
