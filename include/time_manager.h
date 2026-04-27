#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "board.h"
#include "move.h"

#include <chrono>
#include <cstdint>

// All parameters needed to initialise a time search
struct TimeControl {
    int  wtime         = 0;
    int  btime         = 0;
    int  winc          = 0;
    int  binc          = 0;
    int  movetime      = 0;   // fixed ms per move; 0 = not set
    bool infinite      = false;
    Colour side        = WHITE;
    int  move_overhead = 10;  // ms subtracted from available time
};

// TimeManager
//
// Computes a soft limit (target) and hard limit (absolute max).
//
//  * Soft limit  — checked once per completed depth iteration.
//    Scaled down when the best move is stable across iterations,
//    scaled up when the score drops significantly (we're in trouble).
//
//  * Hard limit  — checked by a timer thread in uci.cpp.
//    Acts as an absolute ceiling regardless of move stability.
class TimeManager {
public:
    // Call once when a "go" command is received.
    void init(const TimeControl& tc);

    // Call after each completed depth iteration.
    // Returns true  → soft limit reached, search should stop.
    // Returns false → keep searching.
    bool on_iter(int depth, Move best_move, int score);

    // True when the hard limit has been exceeded.
    // Poll from the timer thread in uci.cpp.
    bool hard_expired() const;

    // Milliseconds since init() was called.
    int64_t elapsed_ms() const;

    // Soft / hard limits (ms) — exposed for the timer thread.
    int64_t soft_ms() const { return soft_ms_; }
    int64_t hard_ms() const { return hard_ms_; }

private:
    std::chrono::steady_clock::time_point start_;

    int64_t soft_ms_ = 0;
    int64_t hard_ms_ = 0;

    // Move-stability tracking
    Move  prev_best_     = MOVE_NONE;
    int   stable_iters_  = 0;

    // Score-drop tracking
    int   prev_score_    = 0;
    bool  score_inited_  = false;
};

extern TimeManager g_time_manager;

#endif
