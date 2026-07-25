#ifndef MOVE_IO_H
#define MOVE_IO_H

#include "board.h"
#include "move.h"

#include <string>

namespace SHAYVERI {

std::string move_to_uci(Move move);
Move uci_to_move(Board &board, const std::string &uci);

} // namespace SHAYVERI

#endif // MOVE_IO_H
