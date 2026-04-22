#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "board.h"
#include "move.h"

struct MoveList {
    Move moves[256];
    int count = 0;
    void add(Move m) { moves[count++] = m; }
};

MoveList generate_pseudo_legal_moves(Board &b);

MoveList generate_legal_moves(Board &b); // filters via make_move legality

#endif