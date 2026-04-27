#include "time_manager.h"

#include <algorithm>

TimeManager g_time_manager;

void TimeManager::init(const TimeControl& tc) {
    start_        = std::chrono::steady_clock::now();
    prev_best_    = MOVE_NONE;
    stable_iters_ = 0;
    prev_score_   = 0;
    score_inited_ = false;

    // Infinite / analysis mode
    if (tc.infinite) {
        soft_ms_ = hard_ms_ = 1'000'000'000LL;
        return;
    }

    // Fixed time per move
    if (tc.movetime > 0) {
        I64 safe = static_cast<I64>(std::max(1, tc.movetime - tc.move_overhead));
        soft_ms_ = hard_ms_ = safe;
        return;
    }

    // Normal time control
    int my_time = (tc.side == WHITE) ? tc.wtime : tc.btime;
    int my_inc  = (tc.side == WHITE) ? tc.winc  : tc.binc;

    if (my_time <= 0) {
        // Shouldn't happen, but be safe
        soft_ms_ = hard_ms_ = 100;
        return;
    }

    // Base: assume ~20 moves left, spend 1/20 of clock + half increment
    I64 base = my_time / 20 + my_inc / 2;

    // Hard ceiling: never spend more than 80 % of remaining time
    I64 ceiling = static_cast<I64>(my_time * 0.80) - tc.move_overhead;
    if (ceiling < 1) ceiling = 1;

    // soft  = base      (can be scaled down by stability, up by score drop)
    // hard  = base * 3  (absolute max before we must report bestmove)
    soft_ms_ = std::min(base,       ceiling);
    hard_ms_ = std::min(base * 3, ceiling);

    // Floor values so we always get at least a minimal search
    if (soft_ms_ < 5)  soft_ms_ = 5;
    if (hard_ms_ < 10) hard_ms_ = 10;
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

    // Move stability
    // Each iteration the best move stays the same → engine is more
    // confident; we can stop sooner.
    if (best_move == prev_best_) {
        ++stable_iters_;
    } else {
        stable_iters_ = 0;
        prev_best_    = best_move;
    }

    // Score drop detection
    // If the score has fallen noticeably compared to the previous
    // iteration we're probably dealing with a difficult position
    // (tactic found for opponent, mate threat, etc.) → extend time.
    int score_drop = 0;
    if (score_inited_) {
        score_drop = prev_score_ - score; // positive = our score fell
    }
    prev_score_   = score;
    score_inited_ = true;

    // Compute dynamic scale for soft limit
    double scale = 1.0;

    // Stability bonuses (compound): less time needed
    if (stable_iters_ >= 4) scale *= 0.80;
    if (stable_iters_ >= 8) scale *= 0.80;

    // Score-drop extensions (compound, only meaningful after a few depths)
    if (depth >= 6) {
        if (score_drop >= 30) scale *= 1.50;
        if (score_drop >= 60) scale *= 1.50;
    }

    I64 adjusted = static_cast<I64>(static_cast<double>(soft_ms_) * scale);
    adjusted = std::max(adjusted, I64(5));
    adjusted = std::min(adjusted, hard_ms_);

    return elapsed >= adjusted;
}
