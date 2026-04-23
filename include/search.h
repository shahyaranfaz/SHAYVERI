#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"

struct SearchResult {
    Move best_move = MOVE_NONE;
    int score = 0;
    int depth = 0;
};

SearchResult search(Board &b, int max_depth);

#endif