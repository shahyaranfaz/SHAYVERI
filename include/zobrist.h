#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "board.h"
#include "types.h"

namespace SHAYVERI {
namespace Zobrist {

extern U64 pieces[PIECE_COUNT][64];
extern U64 sides;
extern U64 castlings[16];
extern U64 en_passants[8];

void init();
U64  compute(const Board &b);

} // namespace Zobrist
} // namespace ShayBot

#endif // ZOBRIST_H
