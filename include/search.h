#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"

#include <atomic>

extern std::atomic<bool> g_stop;

struct SearchResult {
    Move best_move = MOVE_NONE;
    int score = 0;
    int depth = 0;
    U64 nodes = 0;
};

std::string move_to_uci(Move m);

Move uci_to_move(Board& b, const std::string& uci);

SearchResult search(Board &b, int max_depth, const U64 *rep_stack, int rep_len);

#endif
