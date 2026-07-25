#ifndef SEE_H
#define SEE_H

#include "board.h"
#include "move.h"

namespace SHAYVERI {

int  see(const Board &b, Move m);
bool see_ge(const Board &b, Move m, int threshold);
int  quiet_see_after_move(Board &b, Square target);

} // namespace SHAYVERI

#endif // SEE_H
