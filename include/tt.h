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

struct alignas(16) TTSlot {
    std::atomic<U64> key_age{0};
    std::atomic<U64> payload{0};
};

struct alignas(64) TTBucket {
    TTSlot entries[TT_BUCKET_SIZE];
};

static_assert(sizeof(TTSlot) == 16);
static_assert(sizeof(TTBucket) == 64);
static_assert(std::atomic<U64>::is_always_lock_free);

class TranspositionTable {
public:
    void resize(SIZE_T mb);
    void clear();
    void new_search();
    void prefetch(U64 key) const;

    const TTEntry *probe(U64 key) const;

    void store(U64 key, int depth, int score, TTFlag flag, Move best,
               int eval = 0, bool has_eval = false);
    void store_eval(U64 key, int eval);

private:
    std::unique_ptr<TTBucket[]> table;
    SIZE_T                     bucket_count = 0;
    SIZE_T                     mask = 0;
    unsigned                   index_bits = 0;
    std::atomic<U8>            generation{0};
};

extern TranspositionTable TT;

} // namespace SHAYVERI

#endif // TT_H
