#ifndef SEE_H
#define SEE_H

#include "board.h"
#include "move.h"

// Returns estimated material gain/loss from capturing on 'to' with 'from'
// Positive = winning
int see(const Board &b, Move m);

inline bool see_ge_zero(const Board &b, Move m) { return see(b, m) >= 0; }

#endif
