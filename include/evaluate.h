#ifndef EVALUATE_H
#define EVALUATE_H

#include "board.h"

// Returns score in centipawns relative to side to move
// Positive = side to move is better
int evaluate(const Board &b);

#endif
