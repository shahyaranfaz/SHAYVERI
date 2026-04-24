#ifndef TT_H
#define TT_H

#include "types.h"
#include "move.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using SIZE_T = std::size_t;

enum TTFlag : U8 { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
    U64 key = 0;
    int score = 0;
    I8 depth = -1;
    U8 flag = TT_EXACT;
    Move best = MOVE_NONE;
};

class TranspositionTable {
public:
    void resize(SIZE_T mb);

    void clear();

    const TTEntry *probe(U64 key) const; // returns nullptr on miss

    TTEntry *probe(U64 key);

    void store(U64 key, int depth, int score, TTFlag flag, Move best);

private:
    std::vector<TTEntry> table;

    SIZE_T mask = 0;
};

extern TranspositionTable TT;

#endif
