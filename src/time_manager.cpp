#include "time_manager.h"

#include "tune.h"

#include <algorithm>

namespace SHAYVERI {

TimeManager g_time_manager;

void TimeManager::init(const TimeControl &tc) {
    start_        = std::chrono::steady_clock::now();
    prev_best_    = MOVE_NONE;
    stable_iters_ = 0;
    prev_score_   = 0;
    score_inited_ = false;
    min_think_ms_ = tc.min_think_ms;

    if (tc.infinite) {
        soft_ms_ = hard_ms_ = 1'000'000'000LL;
        return;
    }

    if (tc.movetime > 0) {
        I64 safe = static_cast<I64>(std::max(1, tc.movetime - tc.move_overhead));
        soft_ms_ = hard_ms_ = safe;
        return;
    }

    int my_time = (tc.side == WHITE) ? tc.wtime : tc.btime;
    int my_inc  = (tc.side == WHITE) ? tc.winc  : tc.binc;

    if (my_time <= 0) {
        soft_ms_ = hard_ms_ = Tune::time_no_clock_ms;
        return;
    }

    const int moves_to_go_min = std::min(Tune::time_moves_to_go_min, Tune::time_moves_to_go_max);
    const int moves_to_go_max = std::max(Tune::time_moves_to_go_min, Tune::time_moves_to_go_max);
    int moves_to_go = (tc.moves_to_go > 0)
        ? std::clamp(tc.moves_to_go, moves_to_go_min, moves_to_go_max)
        : (my_inc > 0 ? Tune::time_default_moves_with_inc : Tune::time_default_moves_no_inc);

    // Base: spend one time-control slice plus useful increment, while keeping
    // enough bank for sudden-death controls.
    I64 base    = my_time / moves_to_go + static_cast<I64>(my_inc * Tune::time_increment_fraction);
    I64 ceiling = static_cast<I64>(my_time * Tune::time_bank_ceiling_fraction) - tc.move_overhead;
    if (ceiling < 1) ceiling = 1;

    soft_ms_ = std::min(base,       ceiling);
    hard_ms_ = std::min(static_cast<I64>(base * Tune::time_hard_bound_multiplier), ceiling);

    if (soft_ms_ < Tune::time_soft_min_ms) soft_ms_ = Tune::time_soft_min_ms;
    if (hard_ms_ < Tune::time_hard_min_ms) hard_ms_ = Tune::time_hard_min_ms;

    if (min_think_ms_ > 0) {
        soft_ms_ = std::max(soft_ms_, static_cast<I64>(min_think_ms_));
        hard_ms_ = std::max(hard_ms_, static_cast<I64>(min_think_ms_));
    }
}

I64 TimeManager::elapsed_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now() - start_).count();
}

bool TimeManager::hard_expired() const {
    return elapsed_ms() >= hard_ms_;
}

bool TimeManager::on_iter(int depth, Move best_move, int score) {
    const I64 elapsed = elapsed_ms();

    const bool best_changed = score_inited_ && best_move != prev_best_;
    if (best_move == prev_best_) {
        ++stable_iters_;
    } else {
        stable_iters_ = 0;
        prev_best_    = best_move;
    }

    int score_drop = 0;
    if (score_inited_)
        score_drop = prev_score_ - score;
    prev_score_   = score;
    score_inited_ = true;

    double scale = 1.0;
    if (stable_iters_ >= Tune::time_stable_first_iterations) scale *= Tune::time_stable_first_scale;
    if (stable_iters_ >= Tune::time_stable_second_iterations) scale *= Tune::time_stable_second_scale;
    if (best_changed && depth >= Tune::time_best_change_min_depth) scale *= Tune::time_best_change_scale;
    if (depth >= Tune::time_eval_drop_min_depth) {
        if (score_drop >= Tune::time_eval_drop_first_cp) scale *= Tune::time_eval_drop_first_scale;
        if (score_drop >= Tune::time_eval_drop_second_cp) scale *= Tune::time_eval_drop_second_scale;
    }

    I64 adjusted = static_cast<I64>(static_cast<double>(soft_ms_) * scale);
    adjusted = std::max(adjusted, static_cast<I64>(Tune::time_soft_min_ms));
    adjusted = std::min(adjusted, hard_ms_);
    if (min_think_ms_ > 0)
        adjusted = std::max(adjusted, static_cast<I64>(min_think_ms_));

    return elapsed >= adjusted;
}

} // namespace SHAYVERI
