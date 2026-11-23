#ifndef NNUE_UPDATE_H
#define NNUE_UPDATE_H

#include "board.h"
#include "make.h"
#include "move.h"
#include "nnue.h"

namespace SHAYVERI {
namespace NNUE {

void update_accumulator(Accumulator &child, const Accumulator &parent,
                        const Board &post, Move m, const Undo &u);

} // namespace NNUE

} // namespace SHAYVERI

#endif // NNUE_UPDATE_H
