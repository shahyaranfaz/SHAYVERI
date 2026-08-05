#include "attacks.h"
#include "board.h"
#include "datagen.h"
#include "datagen_cli.h"
#include "evaluate.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "nnue.h"
#include "opening_book.h"
#include "parse_cli.h"
#include "search.h"
#include "time_manager.h"
#include "tt.h"
#include "tune.h"
#include "uci_output.h"
#include "zobrist.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace SHAYVERI {

// ======
// UCI options (written by setoption, read by go / search)
// ======
static int  g_num_threads      = 1;
static int  g_move_overhead    = 10;   // ms
static bool g_ponder           = false;
static bool g_own_book         = true;
static int  g_book_info_depth  = 8;
static int  g_min_think_ms     = 0;
static std::string g_eval_file = "<embedded>";

// Ponder state.
static std::atomic<bool> g_pondering{false};  // currently in ponder search

// Active search threads, with index 0 as the main thread.
static std::vector<std::thread> smp_threads;
static std::vector<SearchWorker> uci_search_workers;
static SearchContext uci_search_context{TT};

static int active_thread_limit() {
    return static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
}

static void stop_search() {
    uci_search_context.stop = true;
    for (auto &t : smp_threads)
        if (t.joinable()) t.join();
    smp_threads.clear();
    uci_search_context.node_limit = 0;
}

static Move first_legal_move(Board b) {
    return find_first_legal_move(b);
}

static void format_uci_move(Move move, char (&text)[6]) {
    text[0] = '0';
    text[1] = '0';
    text[2] = '0';
    text[3] = '0';
    text[4] = '\0';
    text[5] = '\0';
    if (move == MOVE_NONE) return;

    const Square from = move_from(move);
    const Square to = move_to(move);
    text[0] = static_cast<char>('a' + get_file(from));
    text[1] = static_cast<char>('1' + get_rank(from));
    text[2] = static_cast<char>('a' + get_file(to));
    text[3] = static_cast<char>('1' + get_rank(to));
    switch (move_promo(move)) {
        case KNIGHT: text[4] = 'n'; break;
        case BISHOP: text[4] = 'b'; break;
        case ROOK:   text[4] = 'r'; break;
        case QUEEN:  text[4] = 'q'; break;
        default: break;
    }
}

static bool parse_spin(const std::string &value, int min_value, int max_value, int &out) {
    int parsed = 0;
    if (!ParseCLI::integer(value, parsed)) return false;
    out = std::clamp(parsed, min_value, max_value);
    return true;
}

static void resize_hash_option(const std::string &value) {
    int hash_mb = 0;
    if (!parse_spin(value, 1, 32768, hash_mb)) return;
    try {
        TT.resize(static_cast<SIZE_T>(hash_mb));
    } catch (const std::exception &) {
        // Keep the existing table if the requested size cannot be allocated.
    }
}

struct OptionHandler {
    const char *name;
    void (*apply)(const std::string &);
};

static bool load_eval_file_or_default(const std::string &path, std::string &error);

static bool handle_uci_option(const std::string &name, const std::string &value) {
    static const std::array<OptionHandler, 10> handlers = {{
        {"Hash", resize_hash_option},
        {"ClearHash", [](const std::string &) {
            TT.clear();
            uci_search_context.clear_histories();
        }},
        {"Threads", [](const std::string &v) { parse_spin(v, 1, 512, g_num_threads); }},
        {"UseNNUE", [](const std::string &v) {
            bool enabled;
            if (!ParseCLI::boolean(v, enabled)) return;
            NNUE::set_enabled(enabled);
            TT.clear();
            uci_search_context.clear_histories();
        }},
        {NNUE::UCI_OPTION_NAME, [](const std::string &v) {
            std::string error;
            if (!load_eval_file_or_default(v, error)) {
                std::cout << "info string NNUE load failed: " << error << "\n";
                return;
            }
            g_eval_file = v;
            TT.clear();
            uci_search_context.clear_histories();
            if (NNUE::is_enabled()) NNUE::print_info();
        }},
        {"OwnBook", [](const std::string &v) { ParseCLI::boolean(v, g_own_book); }},
        {"BookInfoDepth", [](const std::string &v) {
            parse_spin(v, 0, 32, g_book_info_depth);
        }},
        {"Ponder", [](const std::string &v) { ParseCLI::boolean(v, g_ponder); }},
        {"MinimumThinkingTime", [](const std::string &v) {
            parse_spin(v, 0, 5000, g_min_think_ms);
        }},
        {"MoveOverhead", [](const std::string &v) {
            parse_spin(v, 0, 5000, g_move_overhead);
        }},
    }};

    const auto handler = std::find_if(handlers.begin(), handlers.end(),
        [&](const OptionHandler &entry) { return name == entry.name; });
    if (handler == handlers.end()) return false;
    handler->apply(value);
    return true;
}

static bool file_exists(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

static bool is_hce_eval_file(const std::string &path) {
    return path == "<hce>";
}

static bool is_embedded_eval_file(const std::string &path) {
    return path.empty() || path == "<embedded>" || path == "<default>";
}

static bool load_eval_file_or_default(const std::string &path, std::string &error) {
    if (is_hce_eval_file(path)) {
        NNUE::set_enabled(false);
        error.clear();
        return true;
    }

    bool loaded = false;
    if (is_embedded_eval_file(path)) {
        loaded = NNUE::load_embedded_default(error);
    } else {
        loaded = NNUE::load(path, error);
    }

    if (loaded) NNUE::set_enabled(true);
    return loaded;
}

static Move find_ponder_move(const Board &root, Move best) {
    if (best == MOVE_NONE) return MOVE_NONE;

    Board b = root;
    Undo u;
    if (!make_move(b, best, u)) return MOVE_NONE;

    if (const TTEntry *pe = TT.probe(b.hash)) {
        if (is_legal_move(b, pe->best)) {
            return pe->best;
        }
    }

    // A legal fallback keeps GUI pondering functional even when the child
    // position was not retained in the transposition table (for example, on
    // an immediate opening-book move or after a very short search).
    return first_legal_move(b);
}

static void format_ponder(Board &b, Move best, char (&text)[32]) {
    text[0] = '\0';
    const Move ponder = find_ponder_move(b, best);
    if (ponder == MOVE_NONE) return;

    char move[6];
    format_uci_move(ponder, move);
    std::snprintf(text, sizeof(text), " ponder %s", move);
}

static void write_search_output(const char *output, int size, std::size_t capacity) {
    std::lock_guard<std::mutex> output_lock(uci_output_mutex);
    std::cout.flush();
    if (size <= 0) return;
    const std::size_t bytes = std::min<std::size_t>(
        static_cast<std::size_t>(size), capacity - 1);
    std::fwrite(output, 1, bytes, stdout);
    std::fflush(stdout);
}

static std::string ponder_suffix(const Board &b, Move best) {
    const Move ponder = find_ponder_move(b, best);
    return ponder == MOVE_NONE ? "" : " ponder " + move_to_uci(ponder);
}

} // namespace SHAYVERI

int main(int argc, char **argv) {
    using namespace SHAYVERI;

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Zobrist::init();
    init_attacks();
    TT.resize(64);
    std::string nnue_error;
    if (!load_eval_file_or_default(g_eval_file, nnue_error)) {
        std::cerr << "[nnue] " << nnue_error << '\n';
        return 1;
    }

    Board b;
    set_startpos(b);

    std::vector<U64> hash_history;
    hash_history.push_back(b.hash);

    if (argc > 1) {
        if (std::string(argv[1]) == "datagen") {
            DatagenOptions options;
            if (!DatagenCLI::parse_args(argc, argv, options) ||
                options.threads <= 0 ||
                (options.target_positions == 0 && options.target_games == 0) ||
                options.output_prefix.empty()) {
                DatagenCLI::print_usage(argv[0]);
                return 1;
            }
            if (options.eval_file.empty())
                options.eval_file = g_eval_file;
            else
                g_eval_file = options.eval_file;
            if (!is_hce_eval_file(g_eval_file) && !is_embedded_eval_file(g_eval_file) &&
                !file_exists(g_eval_file)) {
                std::cerr << "missing eval file: " << g_eval_file << '\n';
                return 1;
            }
            if (!load_eval_file_or_default(g_eval_file, nnue_error)) {
                std::cerr << "[nnue] " << nnue_error << '\n';
                return 1;
            }
            return generate_data(options);
        }
        DatagenCLI::print_usage(argv[0]);
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        // ===== UCI =====
        if (token == "uci") {
            std::lock_guard<std::mutex> output_lock(uci_output_mutex);
            std::cout
                << "id name SHAYVERI\n"
                << "id author Shahyar Anfaz and Averi Wylie\n"
                << "option name Hash                type spin   default 64         min 1 max 32768\n"
                << "option name ClearHash           type button\n"
                << "option name Threads             type spin   default 1          min 1 max 512\n"
                << "option name UseNNUE             type check  default true\n"
                << "option name " << NNUE::UCI_OPTION_NAME
                << "            type string default " << g_eval_file << "\n"
                << "option name OwnBook             type check  default true\n"
                << "option name BookInfoDepth       type spin   default 8          min 0 max 32\n"
                << "option name Ponder              type check  default false\n"
                << "option name MinimumThinkingTime type spin   default 0          min 0 max 5000\n"
                << "option name MoveOverhead        type spin   default 10         min 0 max 5000\n";

            for (auto const& [name, opt] : Tune::tuning_registry) {
                if (opt.type == Tune::TuningOption::INT)
                    std::cout << "option name " << name
                              << " type spin default " << opt.default_str
                              << " min " << opt.min_val
                              << " max " << opt.max_val << "\n";
                else
                    std::cout << "option name " << name
                              << " type string default " << opt.default_str << "\n";
            }
            std::cout << "uciok\n";
        }

        // ===== SETOPTION =====
        else if (token == "setoption") {
            // Options may mutate TT, NNUE, or tuning state. Never change them
            // while a search thread is reading the same state.
            stop_search();
            std::string skip;
            iss >> skip; // "name"
            std::string opt_name;
            std::string word;
            while (iss >> word && word != "value") {
                if (!opt_name.empty()) opt_name += ' ';
                opt_name += word;
            }
            std::string value;
            std::getline(iss, value);
            if (!value.empty() && value.front() == ' ')
                value.erase(value.begin());

            if (!handle_uci_option(opt_name, value) &&
                Tune::handle_setoption(opt_name, value)) {
                TT.clear();
                uci_search_context.clear_histories();
            }
        }

        // ===== ISREADY =====
        else if (token == "isready") {
            std::lock_guard<std::mutex> output_lock(uci_output_mutex);
            std::cout << "readyok\n";
        }

        // ===== UCINEWGAME =====
        else if (token == "ucinewgame") {
            stop_search();
            set_startpos(b);
            hash_history.clear();
            hash_history.push_back(b.hash);
            TT.clear();
            uci_search_context.clear_histories();
            g_pondering.store(false);
        }

        // ===== POSITION =====
        else if (token == "position") {
            stop_search();
            std::string pos;
            iss >> pos;
            if (pos == "startpos") {
                set_startpos(b);
                hash_history.clear();
                hash_history.push_back(b.hash);
            } else if (pos == "fen") {
                std::string fen, part;
                for (int i = 0; i < 6 && iss >> part; ++i)
                    fen += (i ? " " : "") + part;
                if (!set_from_fen(b, fen)) set_startpos(b);
                hash_history.clear();
                hash_history.push_back(b.hash);
            }

            std::string moves_token;
            if (iss >> moves_token && moves_token == "moves") {
                std::string mv;
                while (iss >> mv) {
                    Move m = uci_to_move(b, mv);
                    Undo u;
                    if (m != MOVE_NONE && make_move(b, m, u)) {
                        hash_history.push_back(b.hash);
                        continue;
                    }
                    std::cout << "info string rejected position move " << mv
                              << " at " << get_board_fen(b) << "\n";
                    break;
                }
            }
        }

        // ===== GO =====
        else if (token == "go") {
            bool is_ponder_search = false;
            {
                std::istringstream check(line);
                std::string t;
                while (check >> t)
                    if (t == "ponder") { is_ponder_search = true; break; }
            }

            // Parse go parameters.
            TimeControl tc;
            tc.side          = b.side_to_move;
            tc.move_overhead = g_move_overhead;
            tc.min_think_ms  = g_min_think_ms;
            int fixed_depth  = 0;
            U64 fixed_nodes   = 0;

            std::vector<Move> searchmoves;
            {
                std::istringstream go_iss(line);
                std::string tok;
                go_iss >> tok; // consume "go"
                while (go_iss >> tok) {
                    if      (tok == "infinite")  tc.infinite  = true;
                    else if (tok == "ponder")    { /* handled above */ }
                    else if (tok == "wtime")     go_iss >> tc.wtime;
                    else if (tok == "btime")     go_iss >> tc.btime;
                    else if (tok == "winc")      go_iss >> tc.winc;
                    else if (tok == "binc")      go_iss >> tc.binc;
                    else if (tok == "movetime")  go_iss >> tc.movetime;
                    else if (tok == "depth")     go_iss >> fixed_depth;
                    else if (tok == "nodes")     go_iss >> fixed_nodes;
                    else if (tok == "movestogo") go_iss >> tc.moves_to_go;
                    else if (tok == "searchmoves") {
                        std::string sm_str;
                        while (go_iss >> sm_str) {
                            Move sm = uci_to_move(b, sm_str);
                            if (sm != MOVE_NONE) searchmoves.push_back(sm);
                        }
                    }
                }
            }

            Move book_move = MOVE_NONE;
            bool book_hit = false;
            if (!is_ponder_search && g_own_book) {
                if (const BookEntry *entry = probe_book(b.hash)) {
                    Move candidate = uci_to_move(b, entry->move);
                    const bool allowed = searchmoves.empty() ||
                        std::find(searchmoves.begin(), searchmoves.end(), candidate)
                            != searchmoves.end();

                    // uci_to_move() returns only a legal move, so do not validate
                    // the same book candidate a second time here.
                    if (candidate != MOVE_NONE && allowed) {
                        book_move = candidate;
                        book_hit = true;
                    }
                }
            }

            stop_search();
            uci_search_context.stop = false;
            uci_search_context.nodes = 0;

            if (book_hit && g_book_info_depth == 0) {
                std::cout << "info string book\n"
                          << "bestmove " << move_to_uci(book_move)
                          << ponder_suffix(b, book_move) << "\n";
                std::cout.flush();
                continue;
            }

            bool book_info_search = book_hit && g_book_info_depth > 0;
            if (book_info_search) {
                std::cout << "info string book\n";
                searchmoves.clear();
                searchmoves.push_back(book_move);
            }

            TT.new_search();

            // Ponder searches run until ponderhit supplies the real clock.
            g_pondering.store(is_ponder_search);

            TimeControl real_tc = tc;
            if (fixed_depth > 0 || fixed_nodes > 0 || is_ponder_search)
                tc.infinite = true;
            auto time_manager = std::make_shared<TimeManager>();
            time_manager->init(tc);

            int start = static_cast<int>(hash_history.size()) - 1 - b.half_move;
            if (start < 0) start = 0;
            std::vector<U64> rep(hash_history.begin() + start, hash_history.end());

            Board b_copy    = b;
            int num_thr = std::min(g_num_threads, active_thread_limit());
            if (num_thr != g_num_threads)
                std::cout << "info string Threads clamped to " << num_thr
                          << " by hardware capacity\n";
            uci_search_workers.resize(static_cast<std::size_t>(num_thr));
            I64 hard_ms = time_manager->hard_ms();
            bool pondering = is_ponder_search;
            int root_depth = fixed_depth > 0 ? fixed_depth : 64;
            if (book_info_search)
                root_depth = fixed_depth > 0
                    ? std::min(fixed_depth, g_book_info_depth)
                    : g_book_info_depth;
            if (fixed_nodes > 0 || fixed_depth > 0) {
                smp_threads.push_back(std::thread(
                    [b_copy, rep, searchmoves, root_depth, fixed_nodes,
                     book_info_search, book_move]() mutable {
                        SearchWorker &worker = uci_search_workers[0];
                        uci_search_context.nodes = 0;
                        uci_search_context.node_limit = fixed_nodes;
                        uci_search_context.stop = false;
                        const Board root_board = b_copy;
                        const auto search_start = std::chrono::steady_clock::now();
                        const SearchRequest request{
                            .repetition = rep,
                            .root_moves = searchmoves,
                            .emit_info = true,
                            .retain_history = true,
                        };
                        SearchResult result = fixed_nodes > 0
                            ? search_nodes(
                                uci_search_context, worker, b_copy, fixed_nodes,
                                request)
                            : search(
                                uci_search_context, worker, b_copy, root_depth,
                                request);
                        uci_search_context.stop = true;
                        uci_search_context.node_limit = 0;
                        const auto elapsed_ms =
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - search_start).count();
                        if (book_info_search)
                            result.best_move = book_move;
                        else if (result.best_move == MOVE_NONE)
                            result.best_move = first_legal_move(root_board);
                        const U64 elapsed_for_nps = static_cast<U64>(
                            std::max<I64>(1, elapsed_ms));
                        const U64 nps = result.nodes * 1000 / elapsed_for_nps;

                        char score[32];
                        if (std::abs(result.score) > Tune::MATE_SCORE - Tune::MAX_PLY) {
                            const int mate_dist = Tune::MATE_SCORE - std::abs(result.score);
                            int mate_moves = (mate_dist + 1) / 2;
                            if (result.score < 0) mate_moves = -mate_moves;
                            std::snprintf(score, sizeof(score), "mate %d", mate_moves);
                        } else {
                            std::snprintf(score, sizeof(score), "cp %d", result.score);
                        }

                        char bestmove[6];
                        format_uci_move(result.best_move, bestmove);
                        char ponder[32] = "";
                        format_ponder(b_copy, result.best_move, ponder);

                        char output[512];
                        const int output_size = std::snprintf(
                            output, sizeof(output),
                            "info depth %d seldepth %d score %s time %lld nodes %llu nps %llu hashfull %d tbhits 0\n"
                            "bestmove %s%s\n",
                            result.depth, result.selective_depth, score,
                            static_cast<long long>(elapsed_ms),
                            static_cast<unsigned long long>(result.nodes),
                            static_cast<unsigned long long>(nps),
                            uci_search_context.table.hashfull(), bestmove, ponder);
                        write_search_output(output, output_size, sizeof(output));
                    }));
                continue;
            }

            // Lazy SMP helper threads share the global atomic TT. Their final
            // root result is discarded because they contribute by populating
            // the hash for the main thread.
            for (int t = 1; t < num_thr; ++t) {
                smp_threads.push_back(
                    std::thread([b_copy, rep, t, searchmoves, root_depth]() mutable {
                        SearchWorker &worker =
                            uci_search_workers[static_cast<std::size_t>(t)];
                        std::this_thread::sleep_for(std::chrono::milliseconds(2 * t));
                        search(
                            uci_search_context, worker, b_copy, root_depth,
                            SearchRequest{
                                .repetition = rep,
                                .root_moves = searchmoves,
                                .root_bias = t,
                            });
                    })
                );
            }

            // Main search thread.
            smp_threads.insert(
                smp_threads.begin(),
                std::thread([b_copy, rep, searchmoves, real_tc, hard_ms, pondering, root_depth,
                             book_info_search, book_move, time_manager]() mutable {
                    const Board root_board = b_copy;
                    SearchWorker &worker = uci_search_workers[0];

                    auto timer_active = std::make_shared<std::atomic<bool>>(true);
                    auto clock_ready = std::make_shared<std::atomic<bool>>(!pondering);

                    std::thread timer([hard_ms, real_tc, pondering, timer_active, clock_ready,
                                       time_manager]() {
                        const auto wait_for_limit = [timer_active](I64 limit_ms) {
                            using namespace std::chrono;
                            const auto deadline = steady_clock::now()
                                + milliseconds(std::max<I64>(0, limit_ms));
                            while (timer_active->load()) {
                                const I64 remaining = duration_cast<milliseconds>(
                                    deadline - steady_clock::now()).count();
                                if (remaining <= 0)
                                    break;
                                std::this_thread::sleep_for(
                                    milliseconds(std::min<I64>(5, remaining)));
                            }
                            return timer_active->load();
                        };

                        if (pondering) {
                            while (*timer_active && g_pondering.load()
                                   && !uci_search_context.stop.load())
                                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                            if (!*timer_active || uci_search_context.stop.load())
                                return;

                            time_manager->init(real_tc);
                            clock_ready->store(true);
                            const I64 ponderhit_hard_ms = time_manager->hard_ms();
                            const I64 ponderhit_remaining_ms = std::max<I64>(
                                0, ponderhit_hard_ms - time_manager->elapsed_ms());
                            if (wait_for_limit(ponderhit_remaining_ms))
                                uci_search_context.stop = true;
                            return;
                        }

                        const I64 remaining_ms = std::max<I64>(
                            0, hard_ms - time_manager->elapsed_ms());
                        if (wait_for_limit(remaining_ms))
                            uci_search_context.stop = true;
                    });

                    IterCallback on_iter = [clock_ready, time_manager](
                                               int depth, Move best, int score,
                                               U64, I64,
                                               double best_move_node_fraction) {
                        if (clock_ready->load()
                            && time_manager->on_iter(
                                depth, best, score, best_move_node_fraction))
                            uci_search_context.stop = true;
                    };

                    SearchResult result = search(
                        uci_search_context, worker, b_copy, root_depth,
                        SearchRequest{
                            .repetition = rep,
                            .root_moves = searchmoves,
                            .on_iteration = on_iter,
                            .emit_info = true,
                            .retain_history = true,
                        });

                    // A ponder search that finishes naturally, for example on
                    // a terminal position, must still wait for ponderhit or
                    // stop before returning a move.
                    while (pondering && g_pondering.load()
                           && !uci_search_context.stop.load())
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));

                    *timer_active = false;
                    uci_search_context.stop = true;
                    timer.join();

                    if (book_info_search)
                        result.best_move = book_move;
                    else if (result.best_move == MOVE_NONE)
                        result.best_move = first_legal_move(root_board);
                    uci_search_context.node_limit = 0;
                    char bestmove[6];
                    format_uci_move(result.best_move, bestmove);
                    char ponder_text[32] = "";
                    format_ponder(b_copy, result.best_move, ponder_text);

                    char output[64];
                    const int output_size = std::snprintf(
                        output, sizeof(output), "bestmove %s%s\n",
                        bestmove, ponder_text);
                    write_search_output(output, output_size, sizeof(output));
                })
            );
        }

        // ===== PONDERHIT =====
        else if (token == "ponderhit") {
            g_pondering.store(false);
        }

        // ===== STOP =====
        else if (token == "stop") {
            stop_search();
        }

        // ===== BENCH =====
        else if (token == "bench") {
            std::vector<std::string> bench_fens = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 5 20",
            };

            // Bench is a fresh fixed workload. Stop any active UCI search and
            // discard state that could warm one invocation relative to another.
            stop_search();
            TT.clear();
            uci_search_context.clear_histories();
            uci_search_context.stop = false;

            U64 total_nodes = 0;
            SearchWorker bench_worker;
            auto bench_start = std::chrono::steady_clock::now();
            std::cout << "Running bench...\n";

            for (const auto &fen : bench_fens) {
                Board bench_b;
                set_from_fen(bench_b, fen);
                std::vector<Move> sm;
                uci_search_context.nodes = 0;
                SearchResult res = search(
                    uci_search_context, bench_worker, bench_b, 10,
                    SearchRequest{
                        .root_moves = sm,
                        .emit_info = true,
                        .retain_history = true,
                    });
                total_nodes += res.nodes;
            }

            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - bench_start).count();
            if (elapsed_us == 0) elapsed_us = 1;

            std::cout << "\n===========================\n"
                      << "Nodes: " << total_nodes << "\n"
                      << "Time : " << (static_cast<double>(elapsed_us) / 1000.0) << " ms\n"
                      << "NPS  : " << (total_nodes * 1000000ULL) / static_cast<U64>(elapsed_us) << "\n"
                      << "===========================\n";
        }

        // ===== QUIT =====
        else if (token == "quit") {
            stop_search();
            break;
        }

        std::cout.flush();
    }
    stop_search();
    return 0;
}
