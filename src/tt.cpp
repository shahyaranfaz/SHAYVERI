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
    SIZE_T n = round_down_power2(bytes / sizeof(TTBucket));
    if (n < 1024) n = 1024;
    table.assign(n, TTBucket{});
    mask = n - 1;
    generation = 0;
}

void TranspositionTable::clear() {
    std::fill(table.begin(), table.end(), TTBucket{});
    generation = 0;
}

void TranspositionTable::new_search() {
    ++generation;
}

const TTEntry *TranspositionTable::probe(U64 key) const {
    if (table.empty()) return nullptr;
    const TTBucket &bucket = table[SIZE_T(key) & mask];
    for (const TTEntry &e : bucket.entries)
        if (e.key == key) return &e;
    return nullptr;
}

TTEntry *TranspositionTable::probe(U64 key) {
    if (table.empty()) return nullptr;
    TTBucket &bucket = table[SIZE_T(key) & mask];
    for (TTEntry &e : bucket.entries)
        if (e.key == key) return &e;
    return nullptr;
}

void TranspositionTable::store(U64 key, int depth, int score, TTFlag flag, Move best) {
    if (table.empty()) return;
    TTBucket &bucket = table[SIZE_T(key) & mask];

    for (TTEntry &e : bucket.entries) {
        if (e.key == key) {
            if (depth >= int(e.depth) || flag == TT_EXACT) {
                e.depth = I8(depth);
                e.score = score;
                e.flag  = U8(flag);
                e.age   = generation;
                if (best != MOVE_NONE) e.best = best;
            } else if (best != MOVE_NONE && e.best == MOVE_NONE) {
                e.best = best;
                e.age = generation;
            }
            return;
        }
    }

    TTEntry *replace = &bucket.entries[0];
    for (TTEntry &e : bucket.entries) {
        if (e.key == 0) {
            replace = &e;
            break;
        }
        bool e_old = e.age != generation;
        bool r_old = replace->age != generation;
        if ((e_old && !r_old) || (e_old == r_old && e.depth < replace->depth))
            replace = &e;
    }

    replace->key   = key;
    replace->depth = I8(depth);
    replace->score = score;
    replace->flag  = U8(flag);
    replace->age   = generation;
    replace->best  = best;
    replace->has_eval = false;
    replace->eval = 0;
}

void TranspositionTable::store_eval(U64 key, int eval) {
    if (table.empty()) return;
    TTBucket &bucket = table[SIZE_T(key) & mask];

    for (TTEntry &e : bucket.entries) {
        if (e.key == key) {
            e.eval = eval;
            e.has_eval = true;
            e.age = generation;
            return;
        }
    }

    for (TTEntry &e : bucket.entries) {
        if (e.key == 0) {
            e.key = key;
            e.depth = -1;
            e.score = 0;
            e.flag = TT_EXACT;
            e.age = generation;
            e.best = MOVE_NONE;
            e.eval = eval;
            e.has_eval = true;
            return;
        }
    }
}

} // namespace SHAYVERI
