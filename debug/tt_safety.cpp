#include "tt.h"

#include <iostream>

using SHAYVERI::MOVE_NONE;
using SHAYVERI::TT_EXACT;
using SHAYVERI::TT_LOWER;
using SHAYVERI::TT_UPPER;
using SHAYVERI::TTEntry;
using SHAYVERI::TranspositionTable;
using SHAYVERI::U64;

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
    if (e && !require(e->depth == 4 && e->score == 111 && e->flag == TT_LOWER, "shallow non-exact replaced deeper entry")) ++failures;

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

    if (failures == 0) {
        std::cout << "TT safety suite passed\n";
        return 0;
    }
    std::cerr << "TT safety suite failed with " << failures << " issue(s)\n";
    return 2;
}
