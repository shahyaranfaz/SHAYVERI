#ifndef TT_H
#define TT_H

#include "types.h"
#include "move.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SHAYVERI {

using SIZE_T = std::size_t;

enum TTFlag : U8 { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
    U64  key   = 0;
    int  score = 0;
    I8   depth = -1;
    U8   flag  = TT_EXACT;
    U8   age   = 0;
    Move best  = MOVE_NONE;
};

static constexpr int TT_BUCKET_SIZE = 4;

struct TTBucket {
    TTEntry entries[TT_BUCKET_SIZE];
};

class TranspositionTable {
public:
    void resize(SIZE_T mb);
    void clear();
    void new_search();

    const TTEntry *probe(U64 key) const;
    TTEntry       *probe(U64 key);

    void store(U64 key, int depth, int score, TTFlag flag, Move best);

private:
    std::vector<TTBucket> table;
    SIZE_T               mask = 0;
    U8                   generation = 0;
};

extern TranspositionTable TT;

} // namespace SHAYVERI

#endif // TT_H
