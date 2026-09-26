#ifndef TABLEBASE_H
#define TABLEBASE_H

#include "board.h"
#include "move.h"

#include <string_view>

namespace SHAYVERI::Tablebase {

enum class WDL {
    Failed,
    Loss,
    BlessedLoss,
    Draw,
    CursedWin,
    Win
};

struct RootResult {
    WDL wdl;
    Move move;
    int dtz;
};

bool initialize(std::string_view path);
void shutdown();

unsigned max_pieces();
bool can_probe(const Board &board);

WDL probe_wdl(const Board &board);
RootResult probe_root(const Board &board);

} // namespace SHAYVERI::Tablebase

#endif // TABLEBASE_H
