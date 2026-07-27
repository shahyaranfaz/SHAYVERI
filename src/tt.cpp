#include "tt.h"

#include <algorithm>
#include <bit>
#include <limits>

namespace SHAYVERI {

TranspositionTable TT;
static thread_local TTEntry probe_scratch;

namespace {

constexpr U64 PRESENT_BIT = 1ULL << 63;
constexpr U64 LOCKED_VALUE = 1ULL << 62;
constexpr U64 AGE_MASK = 0xFFULL;
constexpr U64 KEY_MASK = ~(PRESENT_BIT | LOCKED_VALUE | AGE_MASK);
constexpr U64 MOVE_MASK = (1ULL << 17) - 1;

U64 pack_payload(int score, int eval, int depth, TTFlag flag,
                 Move best, bool has_eval) {
    const I16 packed_score = static_cast<I16>(
        std::clamp(score, static_cast<int>(std::numeric_limits<I16>::min()),
                  static_cast<int>(std::numeric_limits<I16>::max())));
    const I16 packed_eval = static_cast<I16>(
        std::clamp(eval, static_cast<int>(std::numeric_limits<I16>::min()),
                  static_cast<int>(std::numeric_limits<I16>::max())));
    const U8 packed_depth =
        static_cast<U8>(std::clamp(depth, -1, 127) + 1);

    return static_cast<U16>(packed_score)
         | (static_cast<U64>(static_cast<U16>(packed_eval)) << 16)
         | ((static_cast<U64>(best) & MOVE_MASK) << 32)
         | (static_cast<U64>(packed_depth) << 49)
         | (static_cast<U64>(flag) << 57)
         | (static_cast<U64>(has_eval ? 1 : 0) << 59);
}

TTEntry unpack_entry(U64 key, U64 key_age, U64 payload) {
    TTEntry entry;
    entry.key = key;
    entry.score = static_cast<I16>(payload & 0xFFFFULL);
    entry.eval = static_cast<I16>((payload >> 16) & 0xFFFFULL);
    entry.best = static_cast<Move>((payload >> 32) & MOVE_MASK);
    entry.depth = static_cast<I8>(((payload >> 49) & 0xFFULL) - 1);
    entry.flag = static_cast<U8>((payload >> 57) & 0x3ULL);
    entry.has_eval = ((payload >> 59) & 1ULL) != 0;
    entry.age = static_cast<U8>(key_age & AGE_MASK);
    return entry;
}

U64 publication_value(U64 key, U8 age, unsigned index_bits) {
    return PRESENT_BIT | ((key >> index_bits) << 8) | age;
}

bool publication_matches(U64 key_age, U64 key, unsigned index_bits) {
    return (key_age & KEY_MASK)
        == (publication_value(key, 0, index_bits) & KEY_MASK);
}

bool read_slot(const TTSlot &slot, U64 key, unsigned index_bits,
               TTEntry &entry, U64 *observed = nullptr) {
    const U64 key_age_before =
        slot.key_age.load(std::memory_order_acquire);
    if ((key_age_before & PRESENT_BIT) == 0
        || !publication_matches(key_age_before, key, index_bits))
        return false;

    const U64 payload = slot.payload.load(std::memory_order_relaxed);
    const U64 key_age_after =
        slot.key_age.load(std::memory_order_acquire);
    if (key_age_before != key_age_after) return false;

    entry = unpack_entry(key, key_age_after, payload);
    if (observed) *observed = key_age_after;
    return true;
}

bool read_slot_any(const TTSlot &slot, unsigned index_bits, SIZE_T bucket_index,
                   TTEntry &entry, U64 &observed) {
    const U64 key_age_before =
        slot.key_age.load(std::memory_order_acquire);
    if ((key_age_before & PRESENT_BIT) == 0) {
        observed = key_age_before;
        return false;
    }

    const U64 payload = slot.payload.load(std::memory_order_relaxed);
    const U64 key_age_after =
        slot.key_age.load(std::memory_order_acquire);
    if (key_age_before != key_age_after) return false;

    const U64 high_key = (key_age_after & KEY_MASK) >> 8;
    const U64 key = (high_key << index_bits) | bucket_index;
    entry = unpack_entry(key, key_age_after, payload);
    observed = key_age_after;
    return true;
}

bool lock_slot(TTSlot &slot, U64 expected) {
    if (expected == LOCKED_VALUE) return false;
    return slot.key_age.compare_exchange_strong(
        expected, LOCKED_VALUE,
        std::memory_order_acquire,
        std::memory_order_relaxed);
}

void publish_slot(TTSlot &slot, const TTEntry &entry,
                  unsigned index_bits) {
    slot.payload.store(
        pack_payload(entry.score, entry.eval, entry.depth,
                     static_cast<TTFlag>(entry.flag),
                     entry.best, entry.has_eval),
        std::memory_order_relaxed);
    slot.key_age.store(
        publication_value(entry.key, entry.age, index_bits),
        std::memory_order_release);
}

SIZE_T round_down_power2(SIZE_T n) {
    SIZE_T p = 1;
    while ((p << 1) <= n) p <<= 1;
    return p;
}

} // namespace

void TranspositionTable::resize(SIZE_T mb) {
    const SIZE_T bytes = mb * 1024ULL * 1024ULL;
    SIZE_T n = round_down_power2(bytes / sizeof(TTBucket));
    if (n < 1024) n = 1024;

    table = std::make_unique<TTBucket[]>(n);
    bucket_count = n;
    mask = n - 1;
    index_bits = std::countr_zero(bucket_count);
    generation.store(0, std::memory_order_relaxed);
}

void TranspositionTable::clear() {
    if (!table) return;
    for (SIZE_T i = 0; i < bucket_count; ++i) {
        for (TTSlot &slot : table[i].entries) {
            U64 expected = slot.key_age.load(std::memory_order_relaxed);
            while (!lock_slot(slot, expected))
                expected = slot.key_age.load(std::memory_order_relaxed);
            slot.payload.store(0, std::memory_order_relaxed);
            slot.key_age.store(0, std::memory_order_release);
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
    if (!table || bucket_count == 0 || key == 0) return nullptr;

    const TTBucket &bucket = table[static_cast<SIZE_T>(key) & mask];
    for (const TTSlot &slot : bucket.entries) {
        TTEntry entry;
        if (read_slot(slot, key, index_bits, entry)) {
            probe_scratch = entry;
            return &probe_scratch;
        }
    }
    return nullptr;
}

void TranspositionTable::store(U64 key, int depth, int score, TTFlag flag,
                               Move best, int eval, bool has_eval) {
    if (!table || bucket_count == 0 || key == 0) return;

    const SIZE_T bucket_index = static_cast<SIZE_T>(key) & mask;
    TTBucket &bucket = table[bucket_index];
    const U8 age = generation.load(std::memory_order_relaxed);

    auto update = [&](TTEntry &entry) {
        bool changed = false;
        if (depth >= static_cast<int>(entry.depth) || flag == TT_EXACT) {
            entry.depth = static_cast<I8>(std::clamp(depth, -1, 127));
            entry.score = score;
            entry.flag = static_cast<U8>(flag);
            if (best != MOVE_NONE) entry.best = best;
            if (has_eval) {
                entry.eval = eval;
                entry.has_eval = true;
            }
            changed = true;
        } else if (best != MOVE_NONE && entry.best == MOVE_NONE) {
            entry.best = best;
            if (has_eval) {
                entry.eval = eval;
                entry.has_eval = true;
            }
            changed = true;
        } else if (has_eval && !entry.has_eval) {
            entry.eval = eval;
            entry.has_eval = true;
            changed = true;
        }
        if (changed) entry.age = age;
    };

    for (int attempt = 0; attempt < 4; ++attempt) {
        for (TTSlot &slot : bucket.entries) {
            TTEntry entry;
            U64 observed;
            if (!read_slot(slot, key, index_bits, entry, &observed)) continue;
            if (!lock_slot(slot, observed)) break;
            update(entry);
            publish_slot(slot, entry, index_bits);
            return;
        }

        TTSlot *victim = nullptr;
        TTEntry victim_entry;
        U64 victim_publication = 0;

        for (TTSlot &slot : bucket.entries) {
            TTEntry candidate;
            U64 observed = 0;
            if (!read_slot_any(
                    slot, index_bits, bucket_index, candidate, observed)) {
                if (observed == 0) {
                    victim = &slot;
                    victim_publication = 0;
                    break;
                }
                continue;
            }

            if (!victim) {
                victim = &slot;
                victim_entry = candidate;
                victim_publication = observed;
                continue;
            }

            const bool candidate_old = candidate.age != age;
            const bool victim_old = victim_entry.age != age;
            if ((candidate_old && !victim_old)
                || (candidate_old == victim_old
                    && candidate.depth < victim_entry.depth)) {
                victim = &slot;
                victim_entry = candidate;
                victim_publication = observed;
            }
        }

        if (!victim || !lock_slot(*victim, victim_publication)) continue;
        if (victim_publication != 0 && victim_entry.key == key) {
            update(victim_entry);
            publish_slot(*victim, victim_entry, index_bits);
            return;
        }

        TTEntry fresh;
        fresh.key = key;
        fresh.depth = static_cast<I8>(std::clamp(depth, -1, 127));
        fresh.score = score;
        fresh.flag = static_cast<U8>(flag);
        fresh.age = age;
        fresh.best = best;
        fresh.has_eval = has_eval;
        fresh.eval = has_eval ? eval : 0;
        publish_slot(*victim, fresh, index_bits);
        return;
    }
}

void TranspositionTable::store_eval(U64 key, int eval) {
    if (!table || bucket_count == 0 || key == 0) return;

    const SIZE_T bucket_index = static_cast<SIZE_T>(key) & mask;
    TTBucket &bucket = table[bucket_index];
    const U8 age = generation.load(std::memory_order_relaxed);

    for (int attempt = 0; attempt < 4; ++attempt) {
        for (TTSlot &slot : bucket.entries) {
            TTEntry entry;
            U64 observed;
            if (!read_slot(slot, key, index_bits, entry, &observed)) continue;
            if (!lock_slot(slot, observed)) break;
            entry.eval = eval;
            entry.has_eval = true;
            entry.age = age;
            publish_slot(slot, entry, index_bits);
            return;
        }

        for (TTSlot &slot : bucket.entries) {
            U64 expected = 0;
            if (!lock_slot(slot, expected)) continue;

            TTEntry fresh;
            fresh.key = key;
            fresh.depth = -1;
            fresh.flag = TT_EXACT;
            fresh.age = age;
            fresh.eval = eval;
            fresh.has_eval = true;
            publish_slot(slot, fresh, index_bits);
            return;
        }
    }
}

} // namespace SHAYVERI
