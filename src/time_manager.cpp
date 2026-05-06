#include "time_manager.h"

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
        I64 safe = I64(std::max(1, tc.movetime - tc.move_overhead));
        soft_ms_ = hard_ms_ = safe;
        return;
    }

    int my_time = (tc.side == WHITE) ? tc.wtime : tc.btime;
    int my_inc  = (tc.side == WHITE) ? tc.winc  : tc.binc;

    if (my_time <= 0) {
        soft_ms_ = hard_ms_ = 100;
        return;
    }

    int moves_to_go = (tc.moves_to_go > 0) ? std::clamp(tc.moves_to_go, 1, 80) : 20;

    // Base: spend one time-control slice plus half increment.
    I64 base    = my_time / moves_to_go + my_inc / 2;
    I64 ceiling = I64(my_time * 0.80) - tc.move_overhead;
    if (ceiling < 1) ceiling = 1;

    soft_ms_ = std::min(base,       ceiling);
    hard_ms_ = std::min(base * 3,   ceiling);

    if (soft_ms_ < 5)  soft_ms_ = 5;
    if (hard_ms_ < 10) hard_ms_ = 10;

    if (min_think_ms_ > 0) {
        soft_ms_ = std::max(soft_ms_, I64(min_think_ms_));
        hard_ms_ = std::max(hard_ms_, I64(min_think_ms_));
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
    if (stable_iters_ >= 4) scale *= 0.80;
    if (stable_iters_ >= 8) scale *= 0.80;
    if (depth >= 6) {
        if (score_drop >= 30) scale *= 1.50;
        if (score_drop >= 60) scale *= 1.50;
    }

    I64 adjusted = I64(double(soft_ms_) * scale);
    adjusted = std::max(adjusted, I64(5));
    adjusted = std::min(adjusted, hard_ms_);
    if (min_think_ms_ > 0)
        adjusted = std::max(adjusted, I64(min_think_ms_));

    return elapsed >= adjusted;
}

} // namespace SHAYVERI
