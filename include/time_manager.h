#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "board.h"
#include "move.h"

#include <array>
#include <chrono>
#include <cstdint>

namespace SHAYVERI {

struct TimeControl {
    int wtime = 0;
    int btime = 0;
    int winc = 0;
    int binc = 0;
    int movetime = 0;
    int moves_to_go = 0;
    int min_think_ms = 0;
    bool infinite = false;
    Colour side = WHITE;
    int move_overhead = 10;
};

// Computes a soft limit (target) and hard limit (absolute ceiling).
//
//  Soft limit: checked once per completed depth. It responds to best-move
//  stability, score drops, root best-move node share, and multi-depth
//  evaluation stability.
//
//  Hard limit: absolute ceiling enforced by a timer thread in uci.cpp.
class TimeManager {
public:
    void init(const TimeControl &tc);

    // Returns true when the soft limit has been reached.
    bool on_iter(int depth, Move best_move, int score, double best_move_node_fraction);

    I64 elapsed_ms() const;

    I64 soft_ms() const { return soft_ms_; }
    I64 hard_ms() const { return hard_ms_; }
    double last_scale() const { return last_scale_; }

private:
    std::chrono::steady_clock::time_point start_;

    I64 soft_ms_ = 0;
    I64 hard_ms_ = 0;
    I64 min_think_ms_ = 0;
    bool fixed_movetime_ = false;

    Move prev_best_ = MOVE_NONE;
    int stable_iters_ = 0;
    int prev_score_ = 0;
    bool score_inited_ = false;

    static constexpr int SCORE_HISTORY_CAPACITY = 16;
    std::array<int, SCORE_HISTORY_CAPACITY> score_history_{};
    int score_history_count_ = 0;
    int score_history_next_ = 0;
    double last_scale_ = 1.0;
};

} // namespace SHAYVERI

#endif // TIME_MANAGER_H
