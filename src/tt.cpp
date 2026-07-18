#include "tt.h"

#include <algorithm>

namespace SHAYVERI {

TranspositionTable TT;
thread_local TranspositionTable *active_tt = &TT;
static thread_local TTEntry probe_scratch;

class BucketWriteGuard {
public:
    explicit BucketWriteGuard(TTBucket &bucket) : bucket_(bucket) {
        U64 expected = bucket_.sequence.load(std::memory_order_relaxed);
        while (true) {
            if ((expected & 1ULL) == 0
                && bucket_.sequence.compare_exchange_weak(
                    expected, expected + 1,
                    std::memory_order_acquire,
                    std::memory_order_relaxed))
                break;
            expected = bucket_.sequence.load(std::memory_order_relaxed);
        }
        stable_sequence_ = expected;
    }

    ~BucketWriteGuard() {
        bucket_.sequence.store(stable_sequence_ + 2, std::memory_order_release);
    }

    BucketWriteGuard(const BucketWriteGuard &) = delete;
    BucketWriteGuard &operator=(const BucketWriteGuard &) = delete;

private:
    TTBucket &bucket_;
    U64 stable_sequence_ = 0;
};

TranspositionTable &tt() {
    return *active_tt;
}

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
    U64 key = slot.key.load(std::memory_order_acquire);
    if (key == 0) return false;

    U64 data = slot.data.load(std::memory_order_relaxed);
    U64 meta = slot.meta.load(std::memory_order_relaxed);
    U64 key2 = slot.key.load(std::memory_order_acquire);
    if (key != key2 || key2 == 0) return false;

    out = unpack_entry(key, data, meta);
    return true;
}

static void write_slot(TTSlot &slot, const TTEntry &entry) {
    slot.key.store(0, std::memory_order_release);
    slot.data.store(pack_data(entry.score, entry.eval), std::memory_order_relaxed);
    slot.meta.store(pack_meta(entry.depth, entry.flag, entry.age, entry.has_eval, entry.best),
                    std::memory_order_relaxed);
    slot.key.store(entry.key, std::memory_order_release);
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
        BucketWriteGuard guard(bucket);
        for (TTSlot &slot : bucket.entries) {
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

const TTEntry *TranspositionTable::probe(U64 key) const {
    if (!table || bucket_count == 0) return nullptr;

    const TTBucket &bucket = table[static_cast<SIZE_T>(key) & mask];
    for (int attempt = 0; attempt < 3; ++attempt) {
        const U64 sequence_before = bucket.sequence.load(std::memory_order_acquire);
        if (sequence_before & 1ULL) continue;

        TTEntry match;
        bool found = false;
        for (const TTSlot &slot : bucket.entries) {
            TTEntry entry;
            if (read_slot(slot, entry) && entry.key == key) {
                match = entry;
                found = true;
                break;
            }
        }

        const U64 sequence_after = bucket.sequence.load(std::memory_order_acquire);
        if (sequence_before != sequence_after || (sequence_after & 1ULL))
            continue;

        if (found) {
            probe_scratch = match;
            return &probe_scratch;
        }
        return nullptr;
    }
    return nullptr;
}

void TranspositionTable::store(U64 key, int depth, int score, TTFlag flag, Move best) {
    if (!table || bucket_count == 0 || key == 0) return;

    TTBucket &bucket = table[static_cast<SIZE_T>(key) & mask];
    BucketWriteGuard guard(bucket);
    const U8 age = generation.load(std::memory_order_relaxed);

    TTEntry entry;
    for (TTSlot &slot : bucket.entries) {
        if (!read_slot(slot, entry) || entry.key != key)
            continue;

        if (depth >= static_cast<int>(entry.depth) || flag == TT_EXACT) {
            entry.depth = static_cast<I8>(std::clamp(depth, -128, 127));
            entry.score = score;
            entry.flag = static_cast<U8>(flag);
            entry.age = age;
            if (best != MOVE_NONE) entry.best = best;
            write_slot(slot, entry);
        } else if (best != MOVE_NONE && entry.best == MOVE_NONE) {
            entry.best = best;
            entry.age = age;
            write_slot(slot, entry);
        }
        return;
    }

    TTSlot *replace = &bucket.entries[0];
    TTEntry replace_entry;
    bool have_replace = read_slot(*replace, replace_entry);

    for (TTSlot &slot : bucket.entries) {
        TTEntry candidate;
        if (!read_slot(slot, candidate)) {
            replace = &slot;
            break;
        }

        bool c_old = candidate.age != age;
        bool r_old = !have_replace || replace_entry.age != age;
        if ((c_old && !r_old) ||
            (c_old == r_old && candidate.depth < replace_entry.depth)) {
            replace = &slot;
            replace_entry = candidate;
            have_replace = true;
        }
    }

    TTEntry fresh;
    fresh.key = key;
    fresh.depth = static_cast<I8>(std::clamp(depth, -128, 127));
    fresh.score = score;
    fresh.flag = static_cast<U8>(flag);
    fresh.age = age;
    fresh.best = best;
    fresh.has_eval = false;
    fresh.eval = 0;
    write_slot(*replace, fresh);
}

void TranspositionTable::store_eval(U64 key, int eval) {
    if (!table || bucket_count == 0 || key == 0) return;

    TTBucket &bucket = table[static_cast<SIZE_T>(key) & mask];
    BucketWriteGuard guard(bucket);
    const U8 age = generation.load(std::memory_order_relaxed);

    TTEntry entry;
    for (TTSlot &slot : bucket.entries) {
        if (read_slot(slot, entry) && entry.key == key) {
            entry.eval = eval;
            entry.has_eval = true;
            entry.age = age;
            write_slot(slot, entry);
            return;
        }
    }

    for (TTSlot &slot : bucket.entries) {
        if (slot.key.load(std::memory_order_acquire) == 0) {
            TTEntry fresh;
            fresh.key = key;
            fresh.depth = -1;
            fresh.score = 0;
            fresh.flag = TT_EXACT;
            fresh.age = age;
            fresh.best = MOVE_NONE;
            fresh.eval = eval;
            fresh.has_eval = true;
            write_slot(slot, fresh);
            return;
        }
    }
}

} // namespace SHAYVERI
