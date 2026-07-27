#include "tt.h"

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

using SHAYVERI::MOVE_NONE;
using SHAYVERI::TT_EXACT;
using SHAYVERI::TT_LOWER;
using SHAYVERI::TT_UPPER;
using SHAYVERI::TTEntry;
using SHAYVERI::TranspositionTable;
using SHAYVERI::U64;
using SHAYVERI::create_ep_move;

static bool require(bool cond, const char *msg) {
    if (!cond) std::cerr << "[FAIL] " << msg << "\n";
    return cond;
}

int main() {
    TranspositionTable tt;
    tt.resize(1);

    int failures = 0;
    const U64 key = 0x123456789ABCDEF0ULL;

    tt.store(key, 4, 111, TT_LOWER, MOVE_NONE);
    const TTEntry *e = tt.probe(key);
    if (!require(e != nullptr, "probe after initial store")) ++failures;
    if (e && !require(e->depth == 4 && e->score == 111 && e->flag == TT_LOWER, "initial payload mismatch")) ++failures;

    // Shall not replace with shallower non-exact.
    tt.store(key, 2, 222, TT_UPPER, MOVE_NONE);
    e = tt.probe(key);
    if (e
        && !require(
            e->depth == 4 && e->score == 111 && e->flag == TT_LOWER,
            "shallow non-exact replaced deeper entry"))
        ++failures;

    // Exact replaces even if shallower.
    tt.store(key, 2, 333, TT_EXACT, MOVE_NONE);
    e = tt.probe(key);
    if (e && !require(e->depth == 2 && e->score == 333 && e->flag == TT_EXACT, "exact did not replace")) ++failures;

    // Static eval cache attach/update.
    tt.store_eval(key, 47);
    e = tt.probe(key);
    if (e && !require(e->has_eval && e->eval == 47, "store_eval failed")) ++failures;

    // New generation should update age when storing again.
    const unsigned age_before = e ? e->age : 0;
    tt.new_search();
    tt.store(key, 3, 444, TT_LOWER, MOVE_NONE);
    e = tt.probe(key);
    if (e && !require(e->age != age_before, "age did not update on new search store")) ++failures;

    // Empty-key eval insert path.
    const U64 eval_only_key = 0x0FEDCBA987654321ULL;
    tt.store_eval(eval_only_key, -19);
    const TTEntry *ee = tt.probe(eval_only_key);
    if (!require(ee != nullptr, "probe eval-only key")) ++failures;
    if (ee && !require(ee->has_eval && ee->eval == -19, "eval-only payload mismatch")) ++failures;

    // Compact payload boundaries and the full 17-bit move encoding must
    // round-trip exactly.
    const U64 boundary_key = 0x8000000000000222ULL;
    const auto boundary_move = create_ep_move(63, 63);
    tt.store(boundary_key, 127, -32000, TT_UPPER, boundary_move,
             31871, true);
    const TTEntry *boundary = tt.probe(boundary_key);
    if (!require(boundary != nullptr, "probe compact boundary key"))
        ++failures;
    if (boundary
        && !require(boundary->depth == 127
                        && boundary->score == -32000
                        && boundary->eval == 31871
                        && boundary->best == boundary_move
                        && boundary->flag == TT_UPPER
                        && boundary->has_eval,
                    "compact payload boundary mismatch"))
        ++failures;

    // A normal 1 MiB table has 16,384 buckets. These keys therefore collide
    // in one bucket and exercise four-way residency and depth replacement.
    constexpr U64 bucket_stride = 16384;
    const U64 collision_base = 0x1234;
    for (int i = 0; i < 4; ++i)
        tt.store(collision_base + static_cast<U64>(i) * bucket_stride,
                 8 - i, 500 + i, TT_LOWER, MOVE_NONE);
    for (int i = 0; i < 4; ++i) {
        const TTEntry *collision =
            tt.probe(collision_base + static_cast<U64>(i) * bucket_stride);
        if (!require(collision != nullptr, "four-way collision entry missing"))
            ++failures;
    }
    const U64 replacement_key = collision_base + 4 * bucket_stride;
    tt.store(replacement_key, 9, 600, TT_EXACT, MOVE_NONE);
    if (!require(tt.probe(replacement_key) != nullptr,
                 "collision replacement entry missing"))
        ++failures;
    if (!require(tt.probe(collision_base + 3 * bucket_stride) == nullptr,
                 "shallowest collision entry was not replaced"))
        ++failures;

    // Multiple search threads share the TT. A probe must never observe the
    // score/depth pair from two different writers as one entry.
    const U64 shared_key = 0x55AA55AA55AA55AAULL;
    std::atomic<bool> start{false};
    std::atomic<bool> bad_payload{false};
    std::vector<std::thread> writers;
    writers.reserve(4);
    for (int id = 0; id < 4; ++id) {
        writers.emplace_back([&, id] {
            while (!start.load(std::memory_order_acquire)) {
            }
            const int depth = 20 + id;
            const int score = 1000 + id;
            for (int i = 0; i < 50000; ++i)
                tt.store(shared_key, depth, score, TT_EXACT, MOVE_NONE);
        });
    }

    start.store(true, std::memory_order_release);
    for (int i = 0; i < 200000; ++i) {
        const TTEntry *shared = tt.probe(shared_key);
        if (!shared) continue;
        const int id = shared->depth - 20;
        if (id < 0 || id >= 4 || shared->score != 1000 + id) {
            bad_payload.store(true, std::memory_order_relaxed);
            break;
        }
    }
    for (auto &writer : writers) writer.join();
    if (!require(!bad_payload.load(), "concurrent writers produced mixed payload")) ++failures;

    if (failures == 0) {
        std::cout << "TT safety suite passed\n";
        return 0;
    }
    std::cerr << "TT safety suite failed with " << failures << " issue(s)\n";
    return 2;
}
