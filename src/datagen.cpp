#include "datagen.h"

#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move_gen.h"
#include "search.h"
#include "tt.h"
#include "tune.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace SHAYVERI {

namespace {

constexpr int DATAGEN_OPENING_PLIES = 8;
constexpr U64 DATAGEN_SEARCH_NODES = 5000;
constexpr int DATAGEN_WIN_SCORE = 2000;
constexpr int DATAGEN_WIN_PLIES = 4;
constexpr int DATAGEN_MAX_TRAINING_SCORE = 30000;
constexpr U64 DATAGEN_BASE_SEED = 0x9e3779b97f4a7c15ULL;
constexpr U64 DATAGEN_THREAD_PRIME = 0xbf58476d1ce4e5b9ULL;

struct DatagenCounters {
    std::atomic<U64> total_positions{0};
    std::atomic<U64> total_games{0};
};

struct PlainEntry {
    std::string fen;
    int score_white_pov = 0;
};

enum class GameEnd {
    None,
    Checkmate,
    Draw,
    Adjudication,
};

struct GameResult {
    GameEnd end = GameEnd::None;
    int wdl = 1;
};

void pin_worker_to_core(int id) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(id % std::max(1u, std::thread::hardware_concurrency()), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#else
    (void)id;
#endif
}

bool is_in_check(const Board &b) {
    Square ksq = king_square(b, b.side_to_move);
    return is_square_attacked(b, ksq, flip(b.side_to_move));
}

bool is_capture_or_promo(const Board &b, Move m) {
    if (move_promo(m) != NONE_PTYPE) return true;
    if (is_ep_move(m)) return true;
    return b.get_piece(move_to(m)) != NONE_PIECE;
}

bool is_repetition(const std::vector<U64> &history, U64 key) {
    int seen = 0;
    for (U64 h : history) {
        if (h == key && ++seen >= 3) return true;
    }
    return false;
}

bool has_insufficient_material(const Board &b) {
    if (b.bit_boards[WP] | b.bit_boards[BP] |
        b.bit_boards[WR] | b.bit_boards[BR] |
        b.bit_boards[WQ] | b.bit_boards[BQ]) {
        return false;
    }

    int minor_count = 0;
    U64 minors = b.bit_boards[WN] | b.bit_boards[BN] |
                 b.bit_boards[WB] | b.bit_boards[BB];
    while (minors) {
        pop_lsb(minors);
        ++minor_count;
    }
    return minor_count <= 1;
}

GameResult terminal_result(Board &b, const std::vector<U64> &history) {
    MoveList legal = generate_legal_moves(b);
    if (legal.count == 0) {
        if (!is_in_check(b)) return {GameEnd::Draw, 1};
        return {GameEnd::Checkmate, b.side_to_move == WHITE ? 0 : 2};
    }
    if (b.half_move >= 100) return {GameEnd::Draw, 1};
    if (is_repetition(history, b.hash)) return {GameEnd::Draw, 1};
    if (has_insufficient_material(b)) return {GameEnd::Draw, 1};
    return {};
}

bool play_random_opening(Board &b, std::mt19937_64 &rng, std::vector<U64> &history) {
    set_startpos(b);
    history.clear();
    history.push_back(b.hash);

    for (int ply = 0; ply < DATAGEN_OPENING_PLIES; ++ply) {
        MoveList legal = generate_legal_moves(b);
        if (legal.count == 0) return false;

        std::uniform_int_distribution<int> dist(0, legal.count - 1);
        Move chosen = legal.moves[dist(rng)];

        Undo u;
        if (!make_move(b, chosen, u)) return false;
        history.push_back(b.hash);
    }

    return terminal_result(b, history).end == GameEnd::None;
}

SearchResult fixed_node_search(Board &b, const std::vector<U64> &history) {
    std::vector<Move> search_moves;
    tt().new_search();
    return search_nodes(b, DATAGEN_SEARCH_NODES,
                        history.data(), static_cast<int>(history.size()),
                        search_moves, true);
}

int score_to_white_pov(const Board &b, int score_side_to_move_pov) {
    return b.side_to_move == WHITE ? score_side_to_move_pov : -score_side_to_move_pov;
}

int adjudicated_wdl(int score_white_pov) {
    if (score_white_pov > 0) return 2;
    if (score_white_pov < 0) return 0;
    return 1;
}

bool is_mate_score(int score) {
    return std::abs(score) >= Tune::MATE_SCORE - Tune::MAX_PLY;
}

int training_score(int score_white_pov) {
    return std::clamp(score_white_pov,
                      -DATAGEN_MAX_TRAINING_SCORE,
                      DATAGEN_MAX_TRAINING_SCORE);
}

const char *marlinflow_wdl(int wdl) {
    if (wdl == 2) return "1.0";
    if (wdl == 0) return "0.0";
    return "0.5";
}

bool claim_position(DatagenCounters &counters, U64 target_positions) {
    U64 current = counters.total_positions.load(std::memory_order_relaxed);
    while (current < target_positions) {
        if (counters.total_positions.compare_exchange_weak(
                current, current + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

void write_game_entries(std::ofstream &out,
                        const std::vector<PlainEntry> &entries,
                        int wdl,
                        DatagenCounters &counters,
                        U64 target_positions) {
    for (const PlainEntry &entry : entries) {
        if (!claim_position(counters, target_positions)) return;
        out << entry.fen << " | " << entry.score_white_pov << " | " << marlinflow_wdl(wdl) << '\n';
    }
}

void datagen_worker(int id,
                    U64 target_positions,
                    const std::string output_prefix,
                    DatagenCounters &counters,
                    std::atomic<bool> &failed) {
    pin_worker_to_core(id);

    TranspositionTable local_tt;
    local_tt.resize(16);
    active_tt = &local_tt;

    std::mt19937_64 rng(DATAGEN_BASE_SEED + static_cast<U64>(id) * DATAGEN_THREAD_PRIME);
    std::ofstream out(output_prefix + "_" + std::to_string(id) + ".plain");
    if (!out) {
        std::cerr << "datagen worker " << id << " failed to open output file\n";
        failed.store(true, std::memory_order_relaxed);
        return;
    }

    Board b;
    std::vector<U64> history;
    history.reserve(512);

    while (counters.total_positions.load(std::memory_order_relaxed) < target_positions) {
        if (!play_random_opening(b, rng, history)) continue;

        std::vector<PlainEntry> game_entries;
        game_entries.reserve(256);
        int decisive_plies = 0;
        GameResult result;

        while (result.end == GameEnd::None) {
            result = terminal_result(b, history);
            if (result.end != GameEnd::None) break;

            bool in_check = is_in_check(b);
            SearchResult search_result = fixed_node_search(b, history);

            MoveList legal = generate_legal_moves(b);
            Move chosen = search_result.best_move;
            if (chosen == MOVE_NONE && legal.count > 0) chosen = legal.moves[0];
            if (chosen == MOVE_NONE) {
                result = terminal_result(b, history);
                if (result.end == GameEnd::None) result = {GameEnd::Draw, 1};
                break;
            }

            int score_white = score_to_white_pov(b, search_result.score);
            if (std::abs(score_white) >= DATAGEN_WIN_SCORE) {
                if (++decisive_plies >= DATAGEN_WIN_PLIES) {
                    result = {GameEnd::Adjudication, adjudicated_wdl(score_white)};
                    break;
                }
            } else {
                decisive_plies = 0;
            }

            bool save_position = !in_check &&
                                 !is_capture_or_promo(b, chosen) &&
                                 !is_mate_score(score_white);
            if (save_position)
                game_entries.push_back({get_board_fen(b), training_score(score_white)});

            Undo u;
            if (!make_move(b, chosen, u)) {
                result = {GameEnd::Draw, 1};
                break;
            }
            history.push_back(b.hash);
        }

        write_game_entries(out, game_entries, result.wdl, counters, target_positions);
        U64 games = counters.total_games.fetch_add(1, std::memory_order_relaxed) + 1;
        if (games % 10000 == 0) {
            std::cerr << "datagen games=" << games
                      << " positions=" << counters.total_positions.load(std::memory_order_relaxed)
                      << '\n';
        }
    }

    out.flush();
    active_tt = &TT;
}

} // namespace

int generate_data(int threads, U64 target_positions, const char *output_prefix) {
    if (threads <= 0 || target_positions == 0 || output_prefix == nullptr || *output_prefix == '\0') {
        std::cerr << "invalid datagen arguments\n";
        return 1;
    }

    DatagenCounters counters;
    std::atomic<bool> failed{false};
    std::string prefix(output_prefix);
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(threads));

    std::cerr << "datagen threads=" << threads
              << " target_positions=" << target_positions
              << " output_prefix=" << prefix
              << " nodes=" << DATAGEN_SEARCH_NODES
              << " score_pov=white\n";

    for (int id = 0; id < threads; ++id) {
        workers.emplace_back(datagen_worker, id, target_positions, prefix, std::ref(counters), std::ref(failed));
    }
    for (std::thread &worker : workers) {
        if (worker.joinable()) worker.join();
    }

    std::cerr << "datagen complete games=" << counters.total_games.load(std::memory_order_relaxed)
              << " positions=" << counters.total_positions.load(std::memory_order_relaxed)
              << '\n';
    return failed.load(std::memory_order_relaxed) ? 1 : 0;
}

} // namespace SHAYVERI
