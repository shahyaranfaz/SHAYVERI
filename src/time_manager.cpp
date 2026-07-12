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
    score_history_.fill(0);
    score_history_count_ = 0;
    score_history_next_  = 0;
    last_scale_ = 1.0;
    min_think_ms_ = std::max(0, tc.min_think_ms);

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
    int my_inc  = std::max(0, (tc.side == WHITE) ? tc.winc : tc.binc);

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
    const I64 overhead = std::max(0, tc.move_overhead);
    const I64 raw_clock_ceiling = std::max<I64>(1, static_cast<I64>(my_time) - overhead);
    const I64 bank_ceiling = std::max<I64>(
        1, static_cast<I64>(my_time * std::max(0.0, Tune::time_bank_ceiling_fraction))
            - overhead);
    const I64 ceiling = std::min(raw_clock_ceiling, bank_ceiling);

    I64 base = my_time / moves_to_go
        + static_cast<I64>(my_inc * Tune::time_increment_fraction);

    soft_ms_ = std::min(base,       ceiling);
    hard_ms_ = std::min(static_cast<I64>(base * Tune::time_hard_bound_multiplier), ceiling);

    if (soft_ms_ < Tune::time_soft_min_ms) soft_ms_ = Tune::time_soft_min_ms;
    if (hard_ms_ < Tune::time_hard_min_ms) hard_ms_ = Tune::time_hard_min_ms;

    if (min_think_ms_ > 0) {
        soft_ms_ = std::max(soft_ms_, static_cast<I64>(min_think_ms_));
        hard_ms_ = std::max(hard_ms_, static_cast<I64>(min_think_ms_));
    }

    // In sudden-death controls, switch to a bounded survival allocation once
    // the remaining bank is critically low. Minimum-time preferences must not
    // defeat this cap.
    if (my_inc == 0
        && my_time <= std::max(0, Tune::time_no_inc_emergency_ms)) {
        const I64 emergency_cap = std::max<I64>(
            1, std::min<I64>(ceiling, Tune::time_no_inc_emergency_max_ms));
        soft_ms_ = std::min(soft_ms_, emergency_cap);
        hard_ms_ = std::min(hard_ms_, emergency_cap);
    }

    // This is the final authority. Neither configured minimums nor adaptive
    // scaling may allocate past the safe clock ceiling.
    soft_ms_ = std::clamp<I64>(soft_ms_, 1, ceiling);
    hard_ms_ = std::clamp<I64>(hard_ms_, 1, ceiling);
    soft_ms_ = std::min(soft_ms_, hard_ms_);
}

I64 TimeManager::elapsed_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now() - start_).count();
}

bool TimeManager::hard_expired() const {
    return elapsed_ms() >= hard_ms_;
}

bool TimeManager::on_iter(int depth, Move best_move, int score,
                          double best_move_node_fraction) {
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

    score_history_[score_history_next_] = score;
    score_history_next_ = (score_history_next_ + 1) % SCORE_HISTORY_CAPACITY;
    score_history_count_ = std::min(score_history_count_ + 1, SCORE_HISTORY_CAPACITY);

    double scale = 1.0;
    if (stable_iters_ >= Tune::time_stable_first_iterations) scale *= Tune::time_stable_first_scale;
    if (stable_iters_ >= Tune::time_stable_second_iterations) scale *= Tune::time_stable_second_scale;
    if (best_changed && depth >= Tune::time_best_change_min_depth) scale *= Tune::time_best_change_scale;
    if (depth >= Tune::time_eval_drop_min_depth) {
        if (score_drop >= Tune::time_eval_drop_first_cp) scale *= Tune::time_eval_drop_first_scale;
        if (score_drop >= Tune::time_eval_drop_second_cp) scale *= Tune::time_eval_drop_second_scale;
    }

    if (depth >= Tune::time_node_min_depth
        && best_move_node_fraction > 0.0) {
        const double share = std::clamp(best_move_node_fraction, 0.0, 1.0);
        const double min_scale = std::min(Tune::time_node_min_scale, Tune::time_node_max_scale);
        const double max_scale = std::max(Tune::time_node_min_scale, Tune::time_node_max_scale);
        const double node_scale = std::clamp(
            Tune::time_node_base_scale - Tune::time_node_share_slope * share,
            min_scale, max_scale);
        scale *= node_scale;
    }

    const int eval_window = std::clamp(
        Tune::time_eval_stability_window, 2, SCORE_HISTORY_CAPACITY);
    if (depth >= Tune::time_eval_stability_min_depth
        && score_history_count_ >= eval_window) {
        int min_score = score;
        int max_score = score;
        for (int i = 1; i < eval_window; ++i) {
            const int index = (score_history_next_ - 1 - i + SCORE_HISTORY_CAPACITY)
                % SCORE_HISTORY_CAPACITY;
            min_score = std::min(min_score, score_history_[index]);
            max_score = std::max(max_score, score_history_[index]);
        }
        if (max_score - min_score <= std::max(0, Tune::time_eval_stability_max_range))
            scale *= Tune::time_eval_stability_scale;
    }
    last_scale_ = scale;

    I64 adjusted = static_cast<I64>(static_cast<double>(soft_ms_) * scale);
    adjusted = std::max(adjusted, static_cast<I64>(Tune::time_soft_min_ms));
    if (min_think_ms_ > 0)
        adjusted = std::max(adjusted, static_cast<I64>(min_think_ms_));
    adjusted = std::clamp<I64>(adjusted, 1, hard_ms_);

    return elapsed >= adjusted;
}

} // namespace SHAYVERI
