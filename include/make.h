#ifndef MAKE_H
#define MAKE_H

#include "board.h"
#include "move.h"

struct Undo {
  int castling;
  Square en_passant;
  int half_move;
  int full_move;
  Piece captured;
  bool was_ep;
  bool was_castle;
  U64 hash;
};

bool make_move(Board &b, Move m, Undo &u);

void unmake_move(Board &b, Move m, const Undo &u);

#endif
