#include "tt.h"

#include <algorithm>

namespace SHAYVERI {

TranspositionTable TT;

static SIZE_T round_down_power2(SIZE_T n) {
    SIZE_T p = 1;
    while ((p << 1) <= n) p <<= 1;
    return p;
}

void TranspositionTable::resize(SIZE_T mb) {
    SIZE_T bytes = mb * 1024ULL * 1024ULL;
    SIZE_T n = round_down_power2(bytes / sizeof(TTEntry));
    if (n < 1024) n = 1024;
    table.assign(n, TTEntry{});
    mask = n - 1;
}

void TranspositionTable::clear() {
    std::fill(table.begin(), table.end(), TTEntry{});
}

const TTEntry *TranspositionTable::probe(U64 key) const {
    if (table.empty()) return nullptr;
    const TTEntry &e = table[SIZE_T(key) & mask];
    return (e.key == key) ? &e : nullptr;
}

TTEntry *TranspositionTable::probe(U64 key) {
    if (table.empty()) return nullptr;
    TTEntry &e = table[SIZE_T(key) & mask];
    return (e.key == key) ? &e : nullptr;
}

void TranspositionTable::store(U64 key, int depth, int score, TTFlag flag, Move best) {
    if (table.empty()) return;
    TTEntry &e = table[SIZE_T(key) & mask];
    if (e.key != key || depth >= int(e.depth)) {
        e.key   = key;
        e.depth = I8(depth);
        e.score = score;
        e.flag  = U8(flag);
        e.best  = best;
    }
}

} // namespace ShayBot
