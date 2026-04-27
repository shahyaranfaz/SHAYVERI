#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "board.h"
#include "move.h"

namespace ShayBot {

struct MoveList {
    Move moves[256];
    int  count = 0;
    void add(Move m) { moves[count++] = m; }
};

MoveList generate_pseudo_legal_moves(Board &b);
MoveList generate_legal_moves(Board &b);

} // namespace ShayBot

#endif // MOVE_GEN_H
