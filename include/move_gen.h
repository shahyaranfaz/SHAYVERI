#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include "board.h"
#include "move.h"

namespace SHAYVERI {

struct MoveList {
    Move moves[256];
    int count = 0;
    void add(Move m) { moves[count++] = m; }
};

MoveList generate_pseudo_legal_moves(Board &b);
MoveList generate_pseudo_legal_captures(Board &b);
MoveList generate_legal_moves(Board &b);
MoveList generate_legal_captures(Board &b);
MoveList generate_legal_moves_checked(Board &b);
Move find_first_legal_move(Board &b);
bool is_legal_move(Board &b, Move move);

} // namespace SHAYVERI

#endif // MOVE_GEN_H
