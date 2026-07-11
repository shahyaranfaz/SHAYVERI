#ifndef SEE_H
#define SEE_H

#include "board.h"
#include "move.h"

namespace SHAYVERI {

int  see(const Board &b, Move m);
int  quiet_see(const Board &b, Move m);

} // namespace SHAYVERI

#endif // SEE_H
