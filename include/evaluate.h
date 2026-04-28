#ifndef EVALUATE_H
#define EVALUATE_H

#include "board.h"

namespace SHAYVERI {

// Returns score in centipawns relative to the side to move.
// Positive means the side to move is better.
int evaluate(const Board &b);

} // namespace ShayBot

#endif // EVALUATE_H
