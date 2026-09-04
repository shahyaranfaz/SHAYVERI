#include "datagen.h"

#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move_gen.h"
#include "opening_book.h"
#include "position_rules.h"
#include "search.h"
#include "tt.h"
#include "tune.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace SHAYVERI {

namespace {

constexpr U64 DATAGEN_THREAD_PRIME = 0xbf58476d1ce4e5b9ULL;

enum class GameEnd {
    None,
    Checkmate,
    Draw,
    Adjudication,
};

enum class FilterReason : int {
    Accepted = 0,
    InCheck,
    CaptureOrPromotion,
    MateScore,
    Score,
    Ply,
    Stride,
    Duplicate,
    Count,
};

struct DataEntry {
    Board board;
    std::string fen;
    int score_white_pov = 0;
};

struct BulletChessBoard {
    U64 occ = 0;
    U8 pcs[16]{};
    I16 score = 0;
    U8 result = 0;
    U8 ksq = 0;
    U8 opp_ksq = 0;
    U8 extra[3]{};
};

static_assert(sizeof(BulletChessBoard) == 32, "Bullet chess records must be 32 bytes");

struct GameResult {
    GameEnd end = GameEnd::None;
    int wdl = 1;
};

struct DatagenCounters {
    std::atomic<U64> total_positions{0};
    std::atomic<U64> claimed_games{0};
    std::atomic<U64> total_games{0};
    std::atomic<U64> game_position_sum{0};
    std::atomic<U64> max_positions_in_game{0};
    std::atomic<U64> twic_book_starts{0};
    std::atomic<U64> external_starts{0};
    std::atomic<U64> invalid_external_starts{0};
    std::atomic<U64> terminal_external_starts{0};
    std::atomic<U64> ended_checkmate{0};
    std::atomic<U64> ended_draw{0};
    std::atomic<U64> ended_adjudication{0};
    std::atomic<U64> adjudicated_white_wins{0};
    std::atomic<U64> adjudicated_black_wins{0};
    std::atomic<U64> adjudication_ply_sum{0};
    std::atomic<U64> max_adjudication_ply{0};
    std::atomic<U64> filtered_in_check{0};
    std::atomic<U64> filtered_capture_or_promo{0};
    std::atomic<U64> filtered_mate_score{0};
    std::atomic<U64> filtered_score{0};
    std::atomic<U64> filtered_ply{0};
    std::atomic<U64> filtered_stride{0};
    std::atomic<U64> duplicate_checks{0};
    std::atomic<U64> filtered_duplicate{0};
    std::array<std::atomic<U64>, 8> cp_buckets{};
    std::array<std::atomic<U64>, 3> wdl_buckets{};
    std::array<std::atomic<U64>, 2> stm_buckets{};
    std::array<std::atomic<U64>, 4> phase_buckets{};
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

bool is_capture_or_promo(const Board &b, Move m) {
    if (move_promo(m) != NONE_PTYPE) return true;
    if (is_ep_move(m)) return true;
    return b.get_piece(move_to(m)) != NONE_PIECE;
}

int popcount64(U64 bb) {
    return __builtin_popcountll(bb);
}

bool structurally_valid_position(const Board &b) {
    if (popcount64(b.bit_boards[WK]) != 1 || popcount64(b.bit_boards[BK]) != 1)
        return false;

    Square white_king = king_square(b, WHITE);
    Square black_king = king_square(b, BLACK);
    if (!is_valid(white_king) || !is_valid(black_king))
        return false;

    int file_delta = std::abs(static_cast<int>(get_file(white_king)) -
                              static_cast<int>(get_file(black_king)));
    int rank_delta = std::abs(static_cast<int>(get_rank(white_king)) -
                              static_cast<int>(get_rank(black_king)));
    if (file_delta <= 1 && rank_delta <= 1)
        return false;

    Square opponent_king = king_square(b, flip(b.side_to_move));
    if (is_square_attacked(b, opponent_king, b.side_to_move))
        return false;

    return true;
}

std::string fen_from_start_line(const std::string &line) {
    std::istringstream iss(line);
    std::string fen;
    std::string token;
    for (int i = 0; i < 6; ++i) {
        if (!(iss >> token)) return "";
        fen += (i == 0 ? "" : " ") + token;
    }
    return fen;
}

int ply_number(const Board &b) {
    return std::max(0, (b.full_move - 1) * 2 + static_cast<int>(b.side_to_move));
}

int material_phase(const Board &b) {
    int phase = 0;
    auto add = [&](Piece p, int value) {
        U64 bb = b.bit_boards[p];
        while (bb) {
            pop_lsb(bb);
            phase += value;
        }
    };

    add(WN, 1); add(BN, 1);
    add(WB, 1); add(BB, 1);
    add(WR, 2); add(BR, 2);
    add(WQ, 4); add(BQ, 4);
    return phase;
}

int phase_bucket(const Board &b) {
    int phase = material_phase(b);
    if (phase >= 18) return 3;
    if (phase >= 10) return 2;
    if (phase >= 1) return 1;
    return 0;
}

int cp_bucket(int cp) {
    int a = std::abs(cp);
    if (a <= 50) return 0;
    if (a <= 100) return 1;
    if (a <= 200) return 2;
    if (a <= 400) return 3;
    if (a <= 800) return 4;
    if (a <= 1200) return 5;
    if (a <= 2000) return 6;
    return 7;
}

GameResult terminal_result(Board &b, const std::vector<U64> &history) {
    MoveList legal = generate_legal_moves(b);
    if (legal.count == 0) {
        if (!PositionRules::is_in_check(b)) return {GameEnd::Draw, 1};
        return {GameEnd::Checkmate, b.side_to_move == WHITE ? 0 : 2};
    }
    if (b.half_move >= 100) return {GameEnd::Draw, 1};
    if (PositionRules::is_threefold_repetition(history, b.hash))
        return {GameEnd::Draw, 1};
    if (PositionRules::has_insufficient_material(b))
        return {GameEnd::Draw, 1};
    return {};
}

SearchResult fixed_node_search(SearchContext &context, SearchWorker &worker,
                               Board &b,
                               const std::vector<U64> &history, U64 node_budget) {
    std::vector<Move> search_moves;
    context.table.new_search();
    return search_nodes(
        context, worker, b, node_budget,
        SearchRequest{
            .repetition = history,
            .root_moves = search_moves,
        });
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

const char *marlinflow_wdl(int wdl) {
    if (wdl == 2) return "1.0";
    if (wdl == 0) return "0.0";
    return "0.5";
}

bool output_is_bullet(const DatagenOptions &options) {
    return options.output_format == "bullet-v1" || options.output_format == "bullet";
}

U8 bullet_piece_code(Piece p, Colour stm) {
    int colour = get_colour(p) == BLACK ? 8 : 0;
    int type = static_cast<int>(get_type(p)) - 1;
    U8 code = static_cast<U8>(colour | type);
    if (stm == BLACK)
        code ^= 8;
    return code;
}

Square bullet_square(Square sq, Colour stm) {
    return stm == BLACK ? (sq ^ 56) : sq;
}

BulletChessBoard make_bullet_record(const Board &b, int score_white_pov, int wdl) {
    BulletChessBoard rec;
    Colour stm = b.side_to_move;
    int score = score_white_pov;
    int result = std::clamp(wdl, 0, 2);
    if (stm == BLACK) {
        score = -score;
        result = 2 - result;
    }

    U64 packed_occ = 0;
    U64 occ = b.occupied;
    while (occ)
        packed_occ |= bb_square(bullet_square(pop_lsb(occ), stm));
    rec.occ = packed_occ;

    int idx = 0;
    while (packed_occ) {
        Square packed_sq = pop_lsb(packed_occ);
        Square sq = bullet_square(packed_sq, stm);
        Piece p = b.get_piece(sq);
        if (p == NONE_PIECE || idx >= 32)
            continue;

        U8 piece = bullet_piece_code(p, stm);
        rec.pcs[idx / 2] |= static_cast<U8>(piece << (4 * (idx & 1)));

        if (piece == 5)
            rec.ksq = static_cast<U8>(packed_sq);
        else if (piece == 13)
            rec.opp_ksq = static_cast<U8>(packed_sq ^ 56);

        ++idx;
    }

    rec.score = static_cast<I16>(std::clamp(score, -32768, 32767));
    rec.result = static_cast<U8>(result);
    return rec;
}

Move choose_opening_move(Board &b, std::mt19937_64 &rng, const MoveList &legal,
                         const DatagenOptions &options) {
    const BookEntry *entry = probe_book(b.hash);
    if (entry != nullptr) {
        std::uniform_real_distribution<double> coin(0.0, 1.0);
        if (coin(rng) < options.book_move_probability) {
            Move book_move = uci_to_move(b, entry->move);
            if (book_move != MOVE_NONE) return book_move;
        }
    }

    std::uniform_int_distribution<int> dist(0, legal.count - 1);
    return legal.moves[dist(rng)];
}

bool play_external_start(Board &b,
                         std::mt19937_64 &rng,
                         std::vector<U64> &history,
                         const std::vector<std::string> &start_fens,
                         DatagenCounters &counters) {
    if (start_fens.empty()) return false;

    std::uniform_int_distribution<size_t> dist(0, start_fens.size() - 1);
    const std::string &fen = start_fens[dist(rng)];
    if (!set_from_fen(b, fen) || !structurally_valid_position(b)) {
        counters.invalid_external_starts++;
        return false;
    }

    history.clear();
    history.push_back(b.hash);
    if (terminal_result(b, history).end != GameEnd::None) {
        counters.terminal_external_starts++;
        return false;
    }

    counters.external_starts++;
    return true;
}

bool play_twic_book_opening(Board &b,
                            std::mt19937_64 &rng,
                            std::vector<U64> &history,
                            const DatagenOptions &options) {
    set_startpos(b);
    history.clear();
    history.push_back(b.hash);

    int min_plies = std::max(0, options.opening_min_plies);
    int max_plies = std::max(min_plies, options.opening_max_plies);
    std::uniform_int_distribution<int> exit_dist(min_plies, max_plies);
    int exit_plies = exit_dist(rng);

    for (int ply = 0; ply < exit_plies; ++ply) {
        MoveList legal = generate_legal_moves(b);
        if (legal.count == 0) return false;

        Move chosen = choose_opening_move(b, rng, legal, options);

        Undo u;
        if (!make_generated_move(b, chosen, u)) return false;
        history.push_back(b.hash);
    }

    return true;
}

bool start_game(Board &b,
                std::mt19937_64 &rng,
                std::vector<U64> &history,
                const DatagenOptions &options,
                const std::vector<std::string> &start_fens,
                DatagenCounters &counters) {
    if (!start_fens.empty() && options.start_file_probability > 0.0) {
        std::uniform_real_distribution<double> coin(0.0, 1.0);
        if (coin(rng) < options.start_file_probability &&
            play_external_start(b, rng, history, start_fens, counters)) {
            return true;
        }
    }

    if (!play_twic_book_opening(b, rng, history, options))
        return false;

    counters.twic_book_starts++;
    return true;
}

void bump_filter(DatagenCounters &counters, FilterReason reason) {
    switch (reason) {
        case FilterReason::InCheck: counters.filtered_in_check++; break;
        case FilterReason::CaptureOrPromotion: counters.filtered_capture_or_promo++; break;
        case FilterReason::MateScore: counters.filtered_mate_score++; break;
        case FilterReason::Score: counters.filtered_score++; break;
        case FilterReason::Ply: counters.filtered_ply++; break;
        case FilterReason::Stride: counters.filtered_stride++; break;
        case FilterReason::Duplicate: counters.filtered_duplicate++; break;
        default: break;
    }
}

FilterReason filter_position(const Board &b,
                             Move chosen,
                             int score_white,
                             int sample_index,
                             const DatagenOptions &options,
                             std::unordered_set<U64> *seen) {
    if (!options.include_checks && PositionRules::is_in_check(b))
        return FilterReason::InCheck;

    if (!options.include_captures && is_capture_or_promo(b, chosen))
        return FilterReason::CaptureOrPromotion;

    if (!options.include_mate_scores && is_mate_score(score_white))
        return FilterReason::MateScore;

    if (std::abs(score_white) > options.max_abs_cp)
        return FilterReason::Score;

    int ply = ply_number(b);
    if (options.min_ply > 0 && ply < options.min_ply)
        return FilterReason::Ply;
    if (options.max_ply > 0 && ply > options.max_ply)
        return FilterReason::Ply;

    if (options.sample_stride > 1 && (sample_index % options.sample_stride) != 0)
        return FilterReason::Stride;

    if (seen != nullptr) {
        if (!seen->insert(b.hash).second)
            return FilterReason::Duplicate;
    }

    return FilterReason::Accepted;
}

bool claim_position(DatagenCounters &counters, U64 target_positions) {
    if (target_positions == 0) {
        counters.total_positions.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

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

bool claim_game(DatagenCounters &counters, U64 target_games) {
    if (target_games == 0) return true;

    U64 current = counters.claimed_games.load(std::memory_order_relaxed);
    while (current < target_games) {
        if (counters.claimed_games.compare_exchange_weak(
                current, current + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

bool should_continue(const DatagenCounters &counters, const DatagenOptions &options) {
    bool positions_left = options.target_positions == 0 ||
        counters.total_positions.load(std::memory_order_relaxed) < options.target_positions;
    bool games_left = options.target_games == 0 ||
        counters.claimed_games.load(std::memory_order_relaxed) < options.target_games;
    return positions_left && games_left;
}

void update_max(std::atomic<U64> &target, U64 value) {
    U64 current = target.load(std::memory_order_relaxed);
    while (value > current &&
           !target.compare_exchange_weak(current, value,
                                         std::memory_order_relaxed,
                                         std::memory_order_relaxed)) {
    }
}

void record_game_end(DatagenCounters &counters, const GameResult &result, int ply) {
    switch (result.end) {
        case GameEnd::Checkmate:
            counters.ended_checkmate++;
            break;
        case GameEnd::Draw:
            counters.ended_draw++;
            break;
        case GameEnd::Adjudication:
            counters.ended_adjudication++;
            counters.adjudication_ply_sum.fetch_add(static_cast<U64>(std::max(0, ply)),
                                                    std::memory_order_relaxed);
            update_max(counters.max_adjudication_ply, static_cast<U64>(std::max(0, ply)));
            if (result.wdl == 2)
                counters.adjudicated_white_wins++;
            else if (result.wdl == 0)
                counters.adjudicated_black_wins++;
            break;
        default:
            break;
    }
}

void record_stats(DatagenCounters &counters, const DataEntry &entry, int wdl, Colour stm, int phase) {
    counters.cp_buckets[cp_bucket(entry.score_white_pov)]++;
    counters.wdl_buckets[std::clamp(wdl, 0, 2)]++;
    counters.stm_buckets[static_cast<int>(stm)]++;
    counters.phase_buckets[std::clamp(phase, 0, 3)]++;
}

U64 write_game_entries(std::ofstream &out,
                       const std::vector<DataEntry> &entries,
                       const std::vector<Colour> &stms,
                       const std::vector<int> &phases,
                       int wdl,
                       DatagenCounters &counters,
                       const DatagenOptions &options,
                        U64 target_positions,
                        std::atomic<bool> &failed) {
    U64 written = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (!claim_position(counters, target_positions)) return written;
        const DataEntry &entry = entries[i];
        if (output_is_bullet(options)) {
            BulletChessBoard rec = make_bullet_record(entry.board, entry.score_white_pov, wdl);
            out.write(reinterpret_cast<const char *>(&rec), sizeof(rec));
        } else {
            out << entry.fen << " | " << entry.score_white_pov << " | " << marlinflow_wdl(wdl) << '\n';
        }
        if (!out) {
            failed.store(true, std::memory_order_relaxed);
            return written;
        }
        record_stats(counters, entry, wdl, stms[i], phases[i]);
        ++written;
    }
    return written;
}

void datagen_worker(int id,
                    const DatagenOptions &options,
                    const std::vector<std::string> &start_fens,
                    DatagenCounters &counters,
                    std::atomic<bool> &failed) {
    pin_worker_to_core(id);

    TranspositionTable local_tt;
    local_tt.resize(16);
    SearchContext search_context{local_tt};
    SearchWorker search_worker;

    std::mt19937_64 rng(options.seed + static_cast<U64>(id) * DATAGEN_THREAD_PRIME);
    const std::string extension = output_is_bullet(options) ? ".bullet.bin" : ".plain";
    std::ios::openmode mode = std::ios::out;
    if (output_is_bullet(options))
        mode |= std::ios::binary;
    std::ofstream out(options.output_prefix + "_" + std::to_string(id) + extension, mode);
    if (!out) {
        std::cerr << "datagen worker " << id << " failed to open output file\n";
        failed.store(true, std::memory_order_relaxed);
        return;
    }
    Board b;
    std::vector<U64> history;
    history.reserve(512);
    std::unordered_set<U64> seen_positions;
    std::unordered_set<U64> *seen_ptr = options.include_duplicates ? nullptr : &seen_positions;

    while (should_continue(counters, options)) {
        std::vector<DataEntry> game_entries;
        std::vector<Colour> stms;
        std::vector<int> phases;
        game_entries.reserve(256);
        stms.reserve(256);
        phases.reserve(256);
        if (!start_game(b, rng, history, options, start_fens, counters)) continue;
        if (!claim_game(counters, options.target_games)) break;

        int decisive_plies = 0;
        int sample_index = 0;
        int samples_this_game = 0;
        GameResult result;

        while (result.end == GameEnd::None) {
            result = terminal_result(b, history);
            if (result.end != GameEnd::None) break;

            int qscore_white = score_to_white_pov(
                b, qsearch_score(search_context, search_worker, b));
            SearchResult search_result = fixed_node_search(
                search_context, search_worker, b, history,
                options.search_nodes);

            Move chosen = search_result.best_move;
            if (chosen == MOVE_NONE) {
                MoveList legal = generate_legal_moves(b);
                if (legal.count > 0) chosen = legal.moves[0];
            }
            if (chosen == MOVE_NONE) {
                result = {GameEnd::Draw, 1};
                break;
            }

            int search_score_white = score_to_white_pov(b, search_result.score);
            if (options.enable_adjudication &&
                std::abs(search_score_white) >= options.adjudication_cp) {
                if (++decisive_plies >= options.adjudication_plies) {
                    result = {GameEnd::Adjudication, adjudicated_wdl(search_score_white)};
                    break;
                }
            } else {
                decisive_plies = 0;
            }

            ++sample_index;
            FilterReason reason = filter_position(
                b, chosen, qscore_white, sample_index, options, seen_ptr);
            if (reason == FilterReason::Accepted &&
                (options.max_samples_per_game <= 0 || samples_this_game < options.max_samples_per_game)) {
                if (seen_ptr != nullptr)
                    counters.duplicate_checks++;
                int phase = phase_bucket(b);
                game_entries.push_back({b, output_is_bullet(options) ? std::string{} : get_board_fen(b), qscore_white});
                stms.push_back(b.side_to_move);
                phases.push_back(phase);
                ++samples_this_game;
            } else if (reason != FilterReason::Accepted) {
                bump_filter(counters, reason);
                if (reason == FilterReason::Duplicate)
                    counters.duplicate_checks++;
            }

            Undo u;
            if (!make_generated_move(b, chosen, u)) {
                result = {GameEnd::Draw, 1};
                break;
            }
            history.push_back(b.hash);
        }

        U64 written = write_game_entries(
            out, game_entries, stms, phases, result.wdl, counters, options,
            options.target_positions, failed);
        counters.game_position_sum.fetch_add(written, std::memory_order_relaxed);
        update_max(counters.max_positions_in_game, written);
        record_game_end(counters, result, ply_number(b));
        U64 games = counters.total_games.fetch_add(1, std::memory_order_relaxed) + 1;
        if (options.print_interval > 0 && games % options.print_interval == 0) {
            std::cerr << "datagen games=" << games
                      << " positions=" << counters.total_positions.load(std::memory_order_relaxed)
                      << " avg_pos_per_game="
                      << static_cast<double>(counters.game_position_sum.load(std::memory_order_relaxed)) /
                             static_cast<double>(std::max<U64>(1, games))
                      << '\n';
        }
        if (failed.load(std::memory_order_relaxed)) break;
    }

    out.flush();
    if (!out) {
        std::cerr << "datagen worker " << id << " failed to flush output file\n";
        failed.store(true, std::memory_order_relaxed);
    }
    out.close();
    if (out.fail()) {
        std::cerr << "datagen worker " << id << " failed to close output file\n";
        failed.store(true, std::memory_order_relaxed);
    }
}

bool write_summary(const DatagenOptions &options, const DatagenCounters &counters) {
    std::ofstream out(options.output_prefix + ".summary.txt");
    if (!out) return false;

    out << "positions " << counters.total_positions.load() << '\n';
    out << "games " << counters.total_games.load() << '\n';
    out << "target_games " << options.target_games << '\n';
    out << "avg_positions_per_game "
        << static_cast<double>(counters.game_position_sum.load()) /
               static_cast<double>(std::max<U64>(1, counters.total_games.load())) << '\n';
    out << "max_positions_from_one_game " << counters.max_positions_in_game.load() << '\n';
    out << "twic_book_starts " << counters.twic_book_starts.load() << '\n';
    out << "external_starts " << counters.external_starts.load() << '\n';
    U64 starts = counters.twic_book_starts.load() + counters.external_starts.load();
    const auto percentage = [](U64 numerator, U64 denominator) {
        return denominator == 0 ? 0.0
            : 100.0 * static_cast<double>(numerator) / static_cast<double>(denominator);
    };
    out << "twic_book_start_pct "
        << percentage(counters.twic_book_starts.load(), starts) << '\n';
    out << "external_start_pct "
        << percentage(counters.external_starts.load(), starts) << '\n';
    out << "format " << options.output_format << '\n';
    out << "threads " << options.threads << '\n';
    out << "nodes " << options.search_nodes << '\n';
    out << "eval_file " << options.eval_file << '\n';
    out << "seed " << options.seed << '\n';
    out << "opening_min_plies " << options.opening_min_plies << '\n';
    out << "opening_max_plies " << options.opening_max_plies << '\n';
    out << "book_move_probability " << options.book_move_probability << '\n';
    out << "start_file " << options.start_file << '\n';
    out << "start_file_probability " << options.start_file_probability << '\n';
    out << "max_abs_cp " << options.max_abs_cp << '\n';
    out << "include_checks " << options.include_checks << '\n';
    out << "include_captures " << options.include_captures << '\n';
    out << "include_mate_scores " << options.include_mate_scores << '\n';
    out << "include_duplicates " << options.include_duplicates << '\n';
    out << "min_ply " << options.min_ply << '\n';
    out << "max_ply " << options.max_ply << '\n';
    out << "sample_stride " << options.sample_stride << '\n';
    out << "max_samples_per_game " << options.max_samples_per_game << '\n';
    out << "enable_adjudication " << options.enable_adjudication << '\n';
    out << "adjudication_cp " << options.adjudication_cp << '\n';
    out << "adjudication_plies " << options.adjudication_plies << '\n';
    out << "print_interval " << options.print_interval << '\n';

    out << "\ngame_ends\n";
    out << "checkmate " << counters.ended_checkmate.load() << '\n';
    out << "draw " << counters.ended_draw.load() << '\n';
    out << "adjudication " << counters.ended_adjudication.load() << '\n';
    out << "adjudicated_white_wins " << counters.adjudicated_white_wins.load() << '\n';
    out << "adjudicated_black_wins " << counters.adjudicated_black_wins.load() << '\n';
    out << "avg_adjudication_ply "
        << static_cast<double>(counters.adjudication_ply_sum.load()) /
               static_cast<double>(std::max<U64>(1, counters.ended_adjudication.load())) << '\n';
    out << "max_adjudication_ply " << counters.max_adjudication_ply.load() << '\n';

    out << "\nfilters\n";
    out << "invalid_external_start " << counters.invalid_external_starts.load() << '\n';
    out << "terminal_external_start " << counters.terminal_external_starts.load() << '\n';
    out << "in_check " << counters.filtered_in_check.load() << '\n';
    out << "capture_or_promo " << counters.filtered_capture_or_promo.load() << '\n';
    out << "mate_score " << counters.filtered_mate_score.load() << '\n';
    out << "score " << counters.filtered_score.load() << '\n';
    out << "ply " << counters.filtered_ply.load() << '\n';
    out << "stride " << counters.filtered_stride.load() << '\n';
    out << "duplicate " << counters.filtered_duplicate.load() << '\n';
    out << "duplicate_checks " << counters.duplicate_checks.load() << '\n';
    out << "duplicate_rate_pct "
        << percentage(counters.filtered_duplicate.load(), counters.duplicate_checks.load()) << '\n';

    out << "\ncp_abs_buckets\n";
    static constexpr const char *CP_NAMES[8] = {
        "000-050", "051-100", "101-200", "201-400",
        "401-800", "801-1200", "1201-2000", "2000+",
    };
    for (int i = 0; i < 8; ++i)
        out << CP_NAMES[i] << ' ' << counters.cp_buckets[i].load() << '\n';

    out << "\nwdl\n";
    out << "0.0 " << counters.wdl_buckets[0].load() << '\n';
    out << "0.5 " << counters.wdl_buckets[1].load() << '\n';
    out << "1.0 " << counters.wdl_buckets[2].load() << '\n';

    out << "\nside_to_move\n";
    out << "w " << counters.stm_buckets[WHITE].load() << '\n';
    out << "b " << counters.stm_buckets[BLACK].load() << '\n';

    out << "\nphase\n";
    out << "end_0 " << counters.phase_buckets[0].load() << '\n';
    out << "late_1-9 " << counters.phase_buckets[1].load() << '\n';
    out << "middle_10-17 " << counters.phase_buckets[2].load() << '\n';
    out << "opening_18+ " << counters.phase_buckets[3].load() << '\n';
    out.flush();
    if (!out) return false;
    out.close();
    return !out.fail();
}

bool valid_options(const DatagenOptions &options) {
    return options.threads > 0 &&
           (options.target_positions > 0 || options.target_games > 0) &&
           !options.output_prefix.empty() &&
           (options.output_format == "shayveri-plain-v1" ||
            options.output_format == "plain" ||
            options.output_format == "bullet-v1" ||
            options.output_format == "bullet") &&
           options.search_nodes > 0 &&
           options.opening_min_plies >= 0 &&
           options.opening_max_plies >= options.opening_min_plies &&
           options.book_move_probability >= 0.0 &&
           options.book_move_probability <= 1.0 &&
           options.start_file_probability >= 0.0 &&
           options.start_file_probability <= 1.0 &&
           options.max_abs_cp > 0 &&
           options.sample_stride > 0 &&
           (!options.enable_adjudication ||
            (options.adjudication_cp > 0 && options.adjudication_plies > 0));
}

std::vector<std::string> load_start_fens(const DatagenOptions &options, DatagenCounters &counters) {
    std::vector<std::string> fens;
    if (options.start_file.empty()) return fens;

    std::ifstream in(options.start_file);
    if (!in) {
        std::cerr << "failed to open start file: " << options.start_file << '\n';
        return fens;
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string fen = fen_from_start_line(line);
        Board b;
        if (!fen.empty() && set_from_fen(b, fen) && structurally_valid_position(b)) {
            std::vector<U64> history{b.hash};
            if (terminal_result(b, history).end == GameEnd::None) {
                fens.push_back(fen);
            } else {
                counters.terminal_external_starts++;
            }
        } else {
            counters.invalid_external_starts++;
        }
    }

    return fens;
}

} // namespace

int generate_data(const DatagenOptions &options) {
    if (!valid_options(options)) {
        std::cerr << "invalid datagen options\n";
        return 1;
    }

    const std::string done_path = options.output_prefix + ".DONE";
    std::error_code remove_error;
    std::filesystem::remove(done_path, remove_error);
    if (remove_error) {
        std::cerr << "failed to remove stale completion marker: " << done_path << '\n';
        return 1;
    }

    DatagenCounters counters;
    std::vector<std::string> start_fens = load_start_fens(options, counters);
    if (!options.start_file.empty() && start_fens.empty()) {
        std::cerr << "no valid start positions loaded from " << options.start_file << '\n';
        return 1;
    }

    std::atomic<bool> failed{false};
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(options.threads));

    std::cerr << "datagen threads=" << options.threads
              << " target_positions=" << options.target_positions
              << " target_games=" << options.target_games
              << " output_prefix=" << options.output_prefix
              << " format=" << options.output_format
              << " eval_file=" << options.eval_file
              << " nodes=" << options.search_nodes
              << " label=qsearch"
              << " score_pov=white"
              << " max_abs_cp=" << options.max_abs_cp
              << " opening_plies=" << options.opening_min_plies
              << "-" << options.opening_max_plies
              << " start_file_positions=" << start_fens.size()
              << '\n';

    for (int id = 0; id < options.threads; ++id) {
        workers.emplace_back(datagen_worker, id, std::cref(options),
                             std::cref(start_fens),
                             std::ref(counters), std::ref(failed));
    }
    for (std::thread &worker : workers) {
        if (worker.joinable()) worker.join();
    }

    if (failed.load(std::memory_order_relaxed)) return 1;
    if (!write_summary(options, counters)) {
        std::cerr << "failed to write datagen summary\n";
        return 1;
    }

    std::cerr << "datagen complete games=" << counters.total_games.load(std::memory_order_relaxed)
              << " positions=" << counters.total_positions.load(std::memory_order_relaxed)
              << " summary=" << options.output_prefix << ".summary.txt"
              << '\n';
    std::ofstream done(done_path);
    if (!done) return 1;
    done << "positions " << counters.total_positions.load(std::memory_order_relaxed) << '\n';
    done << "games " << counters.total_games.load(std::memory_order_relaxed) << '\n';
    done.flush();
    if (!done) return 1;
    done.close();
    return done.fail() ? 1 : 0;
}

} // namespace SHAYVERI
