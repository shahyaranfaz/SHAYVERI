#include "tt.h"

#include <algorithm>

namespace SHAYVERI {

TranspositionTable TT;
static thread_local TTEntry probe_scratch;

class SlotWriteGuard {
public:
    explicit SlotWriteGuard(TTSlot &slot) : slot_(slot) {
        U64 expected = slot_.sequence.load(std::memory_order_relaxed);
        while (true) {
            if ((expected & 1ULL) == 0
                && slot_.sequence.compare_exchange_weak(
                    expected, expected + 1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed))
                break;
            expected = slot_.sequence.load(std::memory_order_relaxed);
        }
        stable_sequence_ = expected;
    }

    ~SlotWriteGuard() {
        slot_.sequence.store(stable_sequence_ + 2, std::memory_order_release);
    }

    SlotWriteGuard(const SlotWriteGuard &) = delete;
    SlotWriteGuard &operator=(const SlotWriteGuard &) = delete;

private:
    TTSlot &slot_;
    U64 stable_sequence_ = 0;
};

static SIZE_T round_down_power2(SIZE_T n) {
    SIZE_T p = 1;
    while ((p << 1) <= n) p <<= 1;
    return p;
}

static U64 pack_data(int score, int eval) {
    return static_cast<U32>(score) |
           (static_cast<U64>(static_cast<U32>(eval)) << 32);
}

static U64 pack_meta(int depth, U8 flag, U8 age, bool has_eval, Move best) {
    const U8 packed_depth = static_cast<U8>(std::clamp(depth, -128, 127) + 128);
    return static_cast<U64>(best) |
           (static_cast<U64>(packed_depth) << 32) |
           (static_cast<U64>(flag) << 40) |
           (static_cast<U64>(age) << 48) |
           (static_cast<U64>(has_eval ? 1 : 0) << 56);
}

static TTEntry unpack_entry(U64 key, U64 data, U64 meta) {
    TTEntry e;
    e.key = key;
    e.score = static_cast<int>(static_cast<I32>(data & 0xFFFFFFFFULL));
    e.eval = static_cast<int>(static_cast<I32>(data >> 32));
    e.best = static_cast<Move>(meta & 0xFFFFFFFFULL);
    e.depth = static_cast<I8>(static_cast<int>((meta >> 32) & 0xFF) - 128);
    e.flag = static_cast<U8>((meta >> 40) & 0xFF);
    e.age = static_cast<U8>((meta >> 48) & 0xFF);
    e.has_eval = ((meta >> 56) & 1ULL) != 0;
    return e;
}

static bool read_slot(const TTSlot &slot, TTEntry &out) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        const U64 sequence_before =
            slot.sequence.load(std::memory_order_acquire);
        if (sequence_before & 1ULL) continue;

        const U64 key = slot.key.load(std::memory_order_relaxed);
        const U64 data = slot.data.load(std::memory_order_relaxed);
        const U64 meta = slot.meta.load(std::memory_order_relaxed);
        const U64 sequence_after =
            slot.sequence.load(std::memory_order_acquire);
        if (sequence_before != sequence_after || (sequence_after & 1ULL))
            continue;
        if (key == 0) return false;
        out = unpack_entry(key, data, meta);
        return true;
    }
    return false;
}

static bool read_locked_slot(const TTSlot &slot, TTEntry &out) {
    const U64 key = slot.key.load(std::memory_order_relaxed);
    if (key == 0) return false;
    const U64 data = slot.data.load(std::memory_order_relaxed);
    const U64 meta = slot.meta.load(std::memory_order_relaxed);
    out = unpack_entry(key, data, meta);
    return true;
}

static void write_locked_slot(TTSlot &slot, const TTEntry &entry) {
    slot.data.store(pack_data(entry.score, entry.eval), std::memory_order_relaxed);
    slot.meta.store(pack_meta(entry.depth, entry.flag, entry.age, entry.has_eval, entry.best),
                    std::memory_order_relaxed);
    slot.key.store(entry.key, std::memory_order_relaxed);
}

void TranspositionTable::resize(SIZE_T mb) {
    SIZE_T bytes = mb * 1024ULL * 1024ULL;
    SIZE_T n = round_down_power2(bytes / sizeof(TTBucket));
    if (n < 1024) n = 1024;

    auto new_table = std::make_unique<TTBucket[]>(n);
    table = std::move(new_table);
    bucket_count = n;
    mask = n - 1;
    generation.store(0, std::memory_order_relaxed);
}

void TranspositionTable::clear() {
    if (!table) return;
    for (SIZE_T i = 0; i < bucket_count; ++i) {
        TTBucket &bucket = table[i];
        for (TTSlot &slot : bucket.entries) {
            SlotWriteGuard guard(slot);
            slot.key.store(0, std::memory_order_relaxed);
            slot.data.store(0, std::memory_order_relaxed);
            slot.meta.store(0, std::memory_order_relaxed);
        }
    }
    generation.store(0, std::memory_order_relaxed);
}

void TranspositionTable::new_search() {
    generation.fetch_add(1, std::memory_order_relaxed);
}

void TranspositionTable::prefetch(U64 key) const {
    if (!table || bucket_count == 0) return;
    __builtin_prefetch(&table[static_cast<SIZE_T>(key) & mask], 0, 1);
}

const TTEntry *TranspositionTable::probe(U64 key) const {
    if (!table || bucket_count == 0) return nullptr;

    const TTBucket &bucket = table[static_cast<SIZE_T>(key) & mask];
    for (const TTSlot &slot : bucket.entries) {
        TTEntry entry;
        if (read_slot(slot, entry) && entry.key == key) {
            probe_scratch = entry;
            return &probe_scratch;
        }
    }
    return nullptr;
}

void TranspositionTable::store(U64 key, int depth, int score, TTFlag flag, Move best,
                               int eval, bool has_eval) {
    if (!table || bucket_count == 0 || key == 0) return;

    TTBucket &bucket = table[static_cast<SIZE_T>(key) & mask];
    const U8 age = generation.load(std::memory_order_relaxed);

    auto update = [&](TTSlot &slot, TTEntry entry) {
        if (depth >= static_cast<int>(entry.depth) || flag == TT_EXACT) {
            entry.depth = static_cast<I8>(std::clamp(depth, -128, 127));
            entry.score = score;
            entry.flag = static_cast<U8>(flag);
            entry.age = age;
            if (best != MOVE_NONE) entry.best = best;
            if (has_eval) {
                entry.eval = eval;
                entry.has_eval = true;
            }
            write_locked_slot(slot, entry);
        } else if (best != MOVE_NONE && entry.best == MOVE_NONE) {
            entry.best = best;
            entry.age = age;
            if (has_eval) {
                entry.eval = eval;
                entry.has_eval = true;
            }
            write_locked_slot(slot, entry);
        } else if (has_eval && !entry.has_eval) {
            entry.eval = eval;
            entry.has_eval = true;
            entry.age = age;
            write_locked_slot(slot, entry);
        }
    };

    for (int attempt = 0; attempt < 4; ++attempt) {
        for (TTSlot &slot : bucket.entries) {
            TTEntry snapshot;
            if (!read_slot(slot, snapshot) || snapshot.key != key) continue;

            SlotWriteGuard guard(slot);
            TTEntry current;
            if (!read_locked_slot(slot, current) || current.key != key)
                break;
            update(slot, current);
            return;
        }

        TTSlot *replace = nullptr;
        TTEntry replace_entry;
        bool replace_valid = false;

        for (TTSlot &slot : bucket.entries) {
            TTEntry candidate;
            if (!read_slot(slot, candidate)) {
                replace = &slot;
                replace_valid = false;
                break;
            }
            if (!replace) {
                replace = &slot;
                replace_entry = candidate;
                replace_valid = true;
                continue;
            }

            const bool candidate_old = candidate.age != age;
            const bool replace_old = replace_entry.age != age;
            if ((candidate_old && !replace_old)
                || (candidate_old == replace_old
                    && candidate.depth < replace_entry.depth)) {
                replace = &slot;
                replace_entry = candidate;
                replace_valid = true;
            }
        }

        if (!replace) return;
        const U64 expected_key = replace_valid ? replace_entry.key : 0;
        SlotWriteGuard guard(*replace);
        TTEntry current;
        const bool current_valid = read_locked_slot(*replace, current);
        const U64 current_key = current_valid ? current.key : 0;
        if (current_key == key) {
            update(*replace, current);
            return;
        }
        if (current_key != expected_key) continue;

        TTEntry fresh;
        fresh.key = key;
        fresh.depth = static_cast<I8>(std::clamp(depth, -128, 127));
        fresh.score = score;
        fresh.flag = static_cast<U8>(flag);
        fresh.age = age;
        fresh.best = best;
        fresh.has_eval = has_eval;
        fresh.eval = has_eval ? eval : 0;
        write_locked_slot(*replace, fresh);
        return;
    }
}

void TranspositionTable::store_eval(U64 key, int eval) {
    if (!table || bucket_count == 0 || key == 0) return;

    TTBucket &bucket = table[static_cast<SIZE_T>(key) & mask];
    const U8 age = generation.load(std::memory_order_relaxed);

    for (int attempt = 0; attempt < 4; ++attempt) {
        for (TTSlot &slot : bucket.entries) {
            TTEntry snapshot;
            if (!read_slot(slot, snapshot) || snapshot.key != key) continue;

            SlotWriteGuard guard(slot);
            TTEntry current;
            if (!read_locked_slot(slot, current) || current.key != key)
                break;
            current.eval = eval;
            current.has_eval = true;
            current.age = age;
            write_locked_slot(slot, current);
            return;
        }

        for (TTSlot &slot : bucket.entries) {
            TTEntry snapshot;
            if (read_slot(slot, snapshot)) continue;

            SlotWriteGuard guard(slot);
            TTEntry current;
            if (read_locked_slot(slot, current)) {
                if (current.key == key) {
                    current.eval = eval;
                    current.has_eval = true;
                    current.age = age;
                    write_locked_slot(slot, current);
                    return;
                }
                break;
            }

            TTEntry fresh;
            fresh.key = key;
            fresh.depth = -1;
            fresh.score = 0;
            fresh.flag = TT_EXACT;
            fresh.age = age;
            fresh.best = MOVE_NONE;
            fresh.eval = eval;
            fresh.has_eval = true;
            write_locked_slot(slot, fresh);
            return;
        }
    }
}

} // namespace SHAYVERI
