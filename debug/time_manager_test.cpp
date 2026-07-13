#include "time_manager.h"
#include "tune.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace SHAYVERI;

namespace {

void expect_near(double actual, double expected, const char* label) {
    if (std::abs(actual - expected) <= 1e-9)
        return;
    std::cerr << label << ": expected " << expected << ", got " << actual << "\n";
    std::exit(1);
}

void expect_true(bool condition, const char* label) {
    if (condition)
        return;
    std::cerr << label << "\n";
    std::exit(1);
}

void neutralize_existing_scalers() {
    Tune::time_stable_first_iterations = 100;
    Tune::time_stable_second_iterations = 100;
    Tune::time_stable_first_scale = 1.0;
    Tune::time_stable_second_scale = 1.0;
    Tune::time_best_change_scale = 1.0;
    Tune::time_eval_drop_first_scale = 1.0;
    Tune::time_eval_drop_second_scale = 1.0;
}

TimeControl infinite_time_control() {
    TimeControl tc;
    tc.infinite = true;
    return tc;
}

void test_node_share_scaling() {
    neutralize_existing_scalers();

    TimeManager manager;
    manager.init(infinite_time_control());
    manager.on_iter(6, 1, 0, 0.75);
    expect_near(manager.last_scale(), 0.80, "node-share scale");

    manager.init(infinite_time_control());
    manager.on_iter(6, 1, 0, 1.0);
    expect_near(manager.last_scale(), 0.75, "node-share minimum");
}

void test_deep_eval_stability_scaling() {
    neutralize_existing_scalers();
    Tune::time_eval_stability_min_depth = 8;
    Tune::time_eval_stability_window = 4;
    Tune::time_eval_stability_max_range = 20;
    Tune::time_eval_stability_scale = 0.90;

    TimeManager manager;
    manager.init(infinite_time_control());
    manager.on_iter(5, 1, 10, 0.0);
    manager.on_iter(6, 1, 14, 0.0);
    manager.on_iter(7, 1, 8, 0.0);
    manager.on_iter(8, 1, 11, 0.0);
    expect_near(manager.last_scale(), 0.90, "stable eval window");

    manager.on_iter(9, 1, 100, 0.0);
    expect_near(manager.last_scale(), 1.0, "unstable eval window");
}

void test_adaptive_scalers_compose() {
    neutralize_existing_scalers();
    Tune::time_eval_stability_min_depth = 8;
    Tune::time_eval_stability_window = 4;
    Tune::time_eval_stability_max_range = 20;
    Tune::time_eval_stability_scale = 0.90;

    TimeManager manager;
    manager.init(infinite_time_control());
    manager.on_iter(5, 1, 10, 0.75);
    manager.on_iter(6, 1, 12, 0.75);
    manager.on_iter(7, 1, 8, 0.75);
    manager.on_iter(8, 1, 11, 0.75);
    expect_near(manager.last_scale(), 0.72, "combined adaptive scale");
}

void test_fixed_movetime_reserves_overhead() {
    TimeControl tc;
    tc.movetime = 50;
    tc.move_overhead = 10;
    tc.min_think_ms = 500;

    TimeManager manager;
    manager.init(tc);
    expect_true(manager.soft_ms() == 40, "movetime soft limit ignored overhead");
    expect_true(manager.hard_ms() == 40, "movetime hard limit ignored overhead");
}

void test_explicit_moves_to_go() {
    TimeControl tc;
    tc.side = WHITE;
    tc.wtime = 1'000;
    tc.winc = 0;
    tc.moves_to_go = 10;
    tc.move_overhead = 0;

    TimeManager manager;
    manager.init(tc);
    expect_true(manager.soft_ms() == 100, "movestogo soft allocation is incorrect");
    expect_true(manager.hard_ms() == 400, "movestogo hard allocation is incorrect");
}

void test_black_clock_selection() {
    TimeControl tc;
    tc.side = BLACK;
    tc.wtime = 10'000;
    tc.btime = 20;
    tc.binc = 1;
    tc.move_overhead = 10;

    TimeManager manager;
    manager.init(tc);
    expect_true(manager.soft_ms() == 5, "time manager did not select Black's clock");
    expect_true(manager.hard_ms() == 6, "Black hard limit escaped its safe ceiling");
}

void test_safe_clock_ceiling_overrides_minimums() {
    TimeControl tc;
    tc.side = WHITE;
    tc.wtime = 20;
    tc.winc = 1;
    tc.move_overhead = 10;
    tc.min_think_ms = 500;

    TimeManager manager;
    manager.init(tc);

    // floor(20 * 0.82) - 10 = 6 ms. Increment avoids the separate
    // no-increment emergency path so this isolates the final clock ceiling.
    expect_true(manager.soft_ms() == 6, "soft limit escaped safe clock ceiling");
    expect_true(manager.hard_ms() == 6, "hard limit escaped safe clock ceiling");

    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    expect_true(manager.on_iter(1, 1, 0, 0.0),
                "minimum thinking time escaped the hard limit in on_iter");
    expect_true(manager.soft_ms() <= manager.hard_ms(), "soft limit exceeds hard limit");
}

void test_no_increment_emergency() {
    Tune::time_no_inc_emergency_ms = 250;
    Tune::time_no_inc_emergency_max_ms = 10;

    TimeControl tc;
    tc.side = WHITE;
    tc.wtime = 200;
    tc.winc = 0;
    tc.move_overhead = 10;
    tc.min_think_ms = 500;

    TimeManager manager;
    manager.init(tc);
    expect_true(manager.soft_ms() == 10, "no-increment emergency soft cap failed");
    expect_true(manager.hard_ms() == 10, "no-increment emergency hard cap failed");
}

void test_no_increment_clock_survival() {
    Tune::time_no_inc_emergency_ms = 250;
    Tune::time_no_inc_emergency_max_ms = 10;

    int remaining = 1'000;
    int allocations = 0;
    while (remaining > 10) {
        TimeControl tc;
        tc.side = WHITE;
        tc.wtime = remaining;
        tc.winc = 0;
        tc.move_overhead = 10;
        tc.min_think_ms = 0;

        TimeManager manager;
        manager.init(tc);
        expect_true(manager.hard_ms() >= 1, "non-positive hard limit");
        expect_true(manager.hard_ms() + tc.move_overhead <= remaining,
                    "no-increment allocation consumes the reserved clock");

        remaining -= static_cast<int>(manager.hard_ms()) + tc.move_overhead;
        ++allocations;
    }
    expect_true(allocations >= 10, "no-increment survival simulation ended too early");
}

} // namespace

int main() {
    test_node_share_scaling();
    test_deep_eval_stability_scaling();
    test_adaptive_scalers_compose();
    test_fixed_movetime_reserves_overhead();
    test_explicit_moves_to_go();
    test_black_clock_selection();
    test_safe_clock_ceiling_overrides_minimums();
    test_no_increment_emergency();
    test_no_increment_clock_survival();
    std::cout << "time manager tests passed\n";
    return 0;
}
