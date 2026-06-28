#ifndef TT_H
#define TT_H

#include "move.h"
#include "types.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace SHAYVERI {

using SIZE_T = std::size_t;

enum TTFlag : U8 { TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2 };

struct TTEntry {
    U64  key   = 0;
    int  score = 0;
    I8   depth = -1;
    U8   flag  = TT_EXACT;
    U8   age   = 0;
    bool has_eval = false;
    int  eval  = 0;
    Move best  = MOVE_NONE;
};

static constexpr int TT_BUCKET_SIZE = 4;

struct TTSlot {
    std::atomic<U64> key{0};
    std::atomic<U64> data{0};
    std::atomic<U64> meta{0};
};

struct TTBucket {
    TTSlot entries[TT_BUCKET_SIZE];
};

class TranspositionTable {
public:
    void resize(SIZE_T mb);
    void clear();
    void new_search();

    const TTEntry *probe(U64 key) const;

    void store(U64 key, int depth, int score, TTFlag flag, Move best);
    void store_eval(U64 key, int eval);

private:
    std::unique_ptr<TTBucket[]> table;
    SIZE_T                     bucket_count = 0;
    SIZE_T                     mask = 0;
    std::atomic<U8>            generation{0};
};

extern TranspositionTable TT;
extern thread_local TranspositionTable *active_tt;

TranspositionTable &tt();

} // namespace SHAYVERI

#endif // TT_H
