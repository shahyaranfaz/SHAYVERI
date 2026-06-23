#include "attacks.h"
#include "board.h"
#include "datagen.h"
#include "evaluate.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "nnue.h"
#include "opening_book.h"
#include "search.h"
#include "time_manager.h"
#include "tt.h"
#include "tune.h"
#include "zobrist.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <cerrno>
#include <cctype>
#include <cstdlib>

namespace SHAYVERI {

// ======
// UCI options (written by setoption, read by go / search)
// ======
static int  g_num_threads   = 1;
static int  g_hash_mb       = 64;
static int  g_move_overhead = 10;   // ms
static bool g_ponder        = false;
static bool g_own_book      = true;
static int  g_min_think_ms  = 0;
static std::string g_eval_file = "SHAYVERI2_2_0.nnue";

// Ponder state.
static Move g_ponder_move =         MOVE_NONE;  // move we are pondering on
static std::atomic<bool> g_pondering{false};  // currently in ponder search

// Active search threads, with index 0 as the main thread.
static std::vector<std::thread> smp_threads;

static void stop_search() {
    g_stop = true;
    for (auto &t : smp_threads)
        if (t.joinable()) t.join();
    smp_threads.clear();
}

static const BookEntry *probe_book(U64 zobrist_key) {
    auto it = std::lower_bound(
        OPENING_BOOK, OPENING_BOOK + OPENING_BOOK_SIZE, zobrist_key,
        [](const BookEntry &e, U64 k) { return e.key < k; });
    if (it != OPENING_BOOK + OPENING_BOOK_SIZE && it->key == zobrist_key)
        return it;
    return nullptr;
}

static bool is_legal_root_move(Board b, Move move) {
    if (move == MOVE_NONE) return false;

    MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i)
        if (legal.moves[i] == move)
            return true;
    return false;
}

static Move first_legal_move(Board b) {
    MoveList legal = generate_legal_moves(b);
    return legal.count > 0 ? legal.moves[0] : MOVE_NONE;
}

static bool parse_bool(const std::string &value) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v == "true" || v == "1" || v == "yes" || v == "on";
}

static bool parse_bool_strict(const std::string &value, bool &out) {
    std::string v = value;
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (v == "true" || v == "1" || v == "yes" || v == "on") {
        out = true;
        return true;
    }
    if (v == "false" || v == "0" || v == "no" || v == "off") {
        out = false;
        return true;
    }
    return false;
}

static std::string trim(std::string value) {
    auto first = std::find_if_not(value.begin(), value.end(),
                                  [](unsigned char c) { return std::isspace(c); });
    auto last = std::find_if_not(value.rbegin(), value.rend(),
                                 [](unsigned char c) { return std::isspace(c); }).base();
    if (first >= last) return "";
    return std::string(first, last);
}

static bool parse_int(const std::string &value, int &out) {
    std::string v = trim(value);
    if (v.empty()) return false;

    char *end = nullptr;
    errno = 0;
    long parsed = std::strtol(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0' || errno == ERANGE)
        return false;
    if (parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max())
        return false;

    out = static_cast<int>(parsed);
    return true;
}

static bool parse_int_64(const std::string &value, U64 &out) {
    std::string v = trim(value);
    if (v.empty()) return false;

    char *end = nullptr;
    errno = 0;
    unsigned long long parsed = std::strtoull(v.c_str(), &end, 10);
    if (end == v.c_str() || *end != '\0' || errno == ERANGE)
        return false;

    out = static_cast<U64>(parsed);
    return true;
}

static bool parse_double_value(const std::string &value, double &out) {
    std::string v = trim(value);
    if (v.empty()) return false;

    char *end = nullptr;
    errno = 0;
    double parsed = std::strtod(v.c_str(), &end);
    if (end == v.c_str() || *end != '\0' || errno == ERANGE)
        return false;

    out = parsed;
    return true;
}

static bool parse_spin(const std::string &value, int min_value, int max_value, int &out) {
    int parsed = 0;
    if (!parse_int(value, parsed)) return false;
    out = std::clamp(parsed, min_value, max_value);
    return true;
}

static void resize_hash_option(const std::string &value) {
    int hash_mb = 0;
    if (!parse_spin(value, 1, 32768, hash_mb)) return;
    try {
        TT.resize(static_cast<SIZE_T>(hash_mb));
        g_hash_mb = hash_mb;
    } catch (const std::exception &) {
        // Keep the existing table if the requested size cannot be allocated.
    }
}

static bool file_exists(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return f.good();
}

static void print_datagen_usage(const char *argv0) {
    std::cerr
        << "usage: " << argv0 << " datagen"
        << " --threads <n>"
        << " [--positions <n>]"
        << " [--games <n>]"
        << " --output-prefix <path>"
        << " [--output-format shayveri-plain-v1|bullet-v1]"
        << " [--eval-file <path>]"
        << " [--nodes <n>]"
        << " [--opening-min-plies <n>]"
        << " [--opening-max-plies <n>]"
        << " [--book-prob <0..1>]"
        << " [--start-file <fen-or-epd-file>]"
        << " [--start-file-prob <0..1>]"
        << " [--seed <n>]"
        << " [--max-abs-cp <n>]"
        << " [--include-checks <bool>]"
        << " [--include-captures <bool>]"
        << " [--include-mate-scores <bool>]"
        << " [--include-duplicates <bool>]"
        << " [--min-ply <n>]"
        << " [--max-ply <n>]"
        << " [--sample-stride <n>]"
        << " [--max-samples-per-game <n>]"
        << " [--enable-adjudication <bool>]"
        << " [--adjudication-cp <n>]"
        << " [--adjudication-plies <n>]"
        << " [--print-interval <games>]\n";
}

static bool require_value(int argc, char **argv, int &i, std::string &value) {
    if (i + 1 >= argc) return false;
    value = argv[++i];
    return true;
}

static bool parse_datagen_args(int argc, char **argv, DatagenOptions &options) {
    for (int i = 2; i < argc; ++i) {
        std::string key = argv[i];
        std::string value;

        auto need_int = [&](int &out) {
            return require_value(argc, argv, i, value) && parse_int(value, out);
        };
        auto need_u64 = [&](U64 &out) {
            return require_value(argc, argv, i, value) && parse_int_64(value, out);
        };
        auto need_double = [&](double &out) {
            return require_value(argc, argv, i, value) && parse_double_value(value, out);
        };
        auto need_bool = [&](bool &out) {
            if (!require_value(argc, argv, i, value)) return false;
            return parse_bool_strict(value, out);
        };

        if (key == "--threads") {
            if (!need_int(options.threads)) return false;
        } else if (key == "--positions") {
            if (!need_u64(options.target_positions)) return false;
        } else if (key == "--games") {
            if (!need_u64(options.target_games)) return false;
        } else if (key == "--output-prefix") {
            if (!require_value(argc, argv, i, options.output_prefix)) return false;
        } else if (key == "--output-format") {
            if (!require_value(argc, argv, i, options.output_format)) return false;
        } else if (key == "--eval-file") {
            if (!require_value(argc, argv, i, options.eval_file)) return false;
        } else if (key == "--nodes") {
            if (!need_u64(options.search_nodes)) return false;
        } else if (key == "--opening-min-plies") {
            if (!need_int(options.opening_min_plies)) return false;
        } else if (key == "--opening-max-plies") {
            if (!need_int(options.opening_max_plies)) return false;
        } else if (key == "--book-prob") {
            if (!need_double(options.book_move_probability)) return false;
        } else if (key == "--start-file") {
            if (!require_value(argc, argv, i, options.start_file)) return false;
        } else if (key == "--start-file-prob") {
            if (!need_double(options.start_file_probability)) return false;
        } else if (key == "--seed") {
            if (!need_u64(options.seed)) return false;
        } else if (key == "--max-abs-cp") {
            if (!need_int(options.max_abs_cp)) return false;
        } else if (key == "--include-checks") {
            if (!need_bool(options.include_checks)) return false;
        } else if (key == "--include-captures") {
            if (!need_bool(options.include_captures)) return false;
        } else if (key == "--include-mate-scores") {
            if (!need_bool(options.include_mate_scores)) return false;
        } else if (key == "--include-duplicates") {
            if (!need_bool(options.include_duplicates)) return false;
        } else if (key == "--min-ply") {
            if (!need_int(options.min_ply)) return false;
        } else if (key == "--max-ply") {
            if (!need_int(options.max_ply)) return false;
        } else if (key == "--sample-stride") {
            if (!need_int(options.sample_stride)) return false;
        } else if (key == "--max-samples-per-game") {
            if (!need_int(options.max_samples_per_game)) return false;
        } else if (key == "--enable-adjudication") {
            if (!need_bool(options.enable_adjudication)) return false;
        } else if (key == "--adjudication-cp") {
            if (!need_int(options.adjudication_cp)) return false;
        } else if (key == "--adjudication-plies") {
            if (!need_int(options.adjudication_plies)) return false;
        } else if (key == "--print-interval") {
            if (!need_u64(options.print_interval)) return false;
        } else {
            return false;
        }
    }

    return true;
}

static std::string ponder_suffix(Board b, Move best) {
    if (!g_ponder || best == MOVE_NONE) return "";

    Undo u;
    if (!make_move(b, best, u)) return "";

    if (const TTEntry *pe = tt().probe(b.hash)) {
        if (is_legal_root_move(b, pe->best)) {
            g_ponder_move = pe->best;
            return " ponder " + move_to_uci(pe->best);
        }
    }
    return "";
}

} // namespace SHAYVERI

int main(int argc, char **argv) {
    using namespace SHAYVERI;

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Zobrist::init();
    init_attacks();
    TT.resize(64);

    if (file_exists(g_eval_file))
        NNUE::load(g_eval_file);

    Board b;
    set_startpos(b);

    std::vector<std::string> move_history;
    std::vector<U64>         hash_history;
    hash_history.push_back(b.hash);

    if (argc > 1) {
        if (std::string(argv[1]) == "datagen") {
            DatagenOptions options;
            if (!parse_datagen_args(argc, argv, options) ||
                options.threads <= 0 ||
                (options.target_positions == 0 && options.target_games == 0) ||
                options.output_prefix.empty()) {
                print_datagen_usage(argv[0]);
                return 1;
            }
            if (options.eval_file.empty())
                options.eval_file = g_eval_file;
            else
                g_eval_file = options.eval_file;
            if (!g_eval_file.empty() && g_eval_file != "<empty>") {
                if (!file_exists(g_eval_file)) {
                    std::cerr << "missing eval file: " << g_eval_file << '\n';
                    return 1;
                }
                NNUE::load(g_eval_file);
            }
            return generate_data(options);
        }
        print_datagen_usage(argv[0]);
        return 1;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        // ===== UCI =====
        if (token == "uci") {
            std::cout
                << "id name SHAYVERI\n"
                << "id author Shahyar Anfaz and Averi Wylie\n"
                << "option name Hash type spin default 64 min 1 max 32768\n"
                << "option name Clear Hash type button\n"
                << "option name Threads type spin default 1 min 1 max 512\n"
                << "option name Ponder type check default false\n"
                << "option name OwnBook type check default true\n"
                << "option name UseNNUE type check default true\n"
                << "option name " << NNUE::UCI_OPTION_NAME
                << " type string default " << g_eval_file << "\n"
                << "option name Minimum Thinking Time type spin default 0 min 0 max 5000\n"
                << "option name Move Overhead type spin default 10 min 0 max 5000\n";

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

            if      (opt_name == "Hash")
                resize_hash_option(value);
            else if (opt_name == "Clear Hash")
                TT.clear();
            else if (opt_name == "Threads") {
                int threads = 0;
                if (parse_spin(value, 1, 512, threads))
                    g_num_threads = threads;
            }
            else if (opt_name == "Ponder")
                g_ponder = parse_bool(value);
            else if (opt_name == "OwnBook")
                g_own_book = parse_bool(value);
            else if (opt_name == "UseNNUE") {
                NNUE::set_enabled(parse_bool(value));
                TT.clear();
            }
            else if (opt_name == NNUE::UCI_OPTION_NAME) {
                g_eval_file = value;
                if (!g_eval_file.empty() && g_eval_file != "<empty>") {
                    NNUE::load(g_eval_file);
                    TT.clear();
                    NNUE::print_info();
                }
            }
            else if (opt_name == "Minimum Thinking Time") {
                int min_think_ms = 0;
                if (parse_spin(value, 0, 5000, min_think_ms))
                    g_min_think_ms = min_think_ms;
            }
            else if (opt_name == "Move Overhead") {
                int move_overhead = 0;
                if (parse_spin(value, 0, 5000, move_overhead))
                    g_move_overhead = move_overhead;
            }
            else
                Tune::handle_setoption(opt_name, value);
        }

        // ===== ISREADY =====
        else if (token == "isready") {
            std::cout << "readyok\n";
        }

        // ===== UCINEWGAME =====
        else if (token == "ucinewgame") {
            stop_search();
            set_startpos(b);
            hash_history.clear();
            hash_history.push_back(b.hash);
            move_history.clear();
            TT.clear();
            g_ponder_move = MOVE_NONE;
            g_pondering.store(false);
        }

        // ===== POSITION =====
        else if (token == "position") {
            stop_search();
            std::string pos;
            iss >> pos;
            move_history.clear();

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
                    if (m != MOVE_NONE) {
                        Undo u;
                        if (make_move(b, m, u)) {
                            move_history.push_back(mv);
                            hash_history.push_back(b.hash);
                        } else {
                            std::cout << "info string rejected position move " << mv
                                      << " at " << get_board_fen(b) << "\n";
                            break;
                        }
                    } else {
                        std::cout << "info string rejected position move " << mv
                                  << " at " << get_board_fen(b) << "\n";
                        break;
                    }
                }
            }
        }

        // ===== GO =====
        else if (token == "go") {
            // Book probe for normal searches.
            bool is_ponder_search = false;
            {
                std::istringstream check(line);
                std::string t;
                while (check >> t)
                    if (t == "ponder") { is_ponder_search = true; break; }
            }

            if (!is_ponder_search && g_own_book) {
                const BookEntry *entry = probe_book(b.hash);
                if (entry) {
                    Move m = uci_to_move(b, entry->move);
                    if (m != MOVE_NONE) {
                        stop_search();
                        g_stop     = false;
                        node_count = 0;

                        int book_eval_cp = static_cast<int>(entry->evaluation * 100.0f);
                        if (b.side_to_move == BLACK) book_eval_cp = -book_eval_cp;

                        std::cout << "info depth 8 score cp " << book_eval_cp
                                  << " pv " << move_to_uci(m) << " \n"
                                  << "bestmove " << move_to_uci(m)
                                  << ponder_suffix(b, m) << "\n";
                        std::cout.flush();
                        continue;
                    }
                }
            }

            stop_search();
            g_stop     = false;
            node_count = 0;
            TT.new_search();

            // Parse go parameters.
            TimeControl tc;
            tc.side          = b.side_to_move;
            tc.move_overhead = g_move_overhead;
            tc.min_think_ms  = g_min_think_ms;
            int fixed_depth  = 0;

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
                    else if (tok == "nodes")     { I64 n; go_iss >> n; }
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

            // Ponder searches run until ponderhit supplies the real clock.
            if (is_ponder_search) {
                g_pondering.store(true);
            } else {
                g_pondering.store(false);
            }

            TimeControl real_tc = tc;
            if (fixed_depth > 0) tc.infinite = true;
            if (is_ponder_search) tc.infinite = true;
            g_time_manager.init(tc);

            int   start = static_cast<int>(hash_history.size()) - 1 - b.half_move;
            if (start < 0) start = 0;
            std::vector<U64> rep(hash_history.begin() + start, hash_history.end());

            Board b_copy    = b;
            int   num_thr   = g_num_threads;
            I64   hard_ms   = g_time_manager.hard_ms();
            bool  pondering = is_ponder_search;
            int   root_depth = fixed_depth > 0 ? fixed_depth : 64;
            int   hash_mb    = g_hash_mb;

            // Lazy SMP helper threads.
            for (int t = 1; t < num_thr; ++t) {
                smp_threads.push_back(
                    std::thread([b_copy, rep, t, searchmoves, root_depth, hash_mb]() mutable {
                        TranspositionTable local_tt;
                        local_tt.resize(static_cast<SIZE_T>(hash_mb));
                        active_tt = &local_tt;
                        std::this_thread::sleep_for(std::chrono::milliseconds(2 * t));
                        int helper_depth = std::max(1, root_depth - (t % 4));
                        search(b_copy, helper_depth,
                               rep.data(), static_cast<int>(rep.size()),
                               searchmoves, nullptr, true, t);
                        active_tt = &TT;
                    })
                );
            }

            // Main search thread.
            smp_threads.insert(
                smp_threads.begin(),
                std::thread([b_copy, rep, searchmoves, real_tc, hard_ms, pondering, root_depth, hash_mb]() mutable {
                    TranspositionTable local_tt;
                    local_tt.resize(static_cast<SIZE_T>(hash_mb));
                    active_tt = &local_tt;
                    local_tt.new_search();

                    auto timer_active = std::make_shared<std::atomic<bool>>(true);

                    std::thread timer([hard_ms, real_tc, pondering, timer_active]() {
                        if (pondering) {
                            while (*timer_active && g_pondering.load() && !g_stop.load())
                                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                            if (!*timer_active || g_stop.load())
                                return;

                            g_time_manager.init(real_tc);
                            const I64 ponderhit_hard_ms = g_time_manager.hard_ms();
                            const I64 slice = 5;
                            I64 slept = 0;
                            while (*timer_active && slept < ponderhit_hard_ms) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(slice));
                                slept += slice;
                            }
                            if (*timer_active) g_stop = true;
                            return;
                        }

                        const I64 slice = 5;
                        I64 slept = 0;
                        while (*timer_active && slept < hard_ms) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
                            slept += slice;
                        }
                        if (*timer_active) g_stop = true;
                    });

                    IterCallback on_iter = [](int depth, Move best, int score, U64, I64) {
                        if (!g_pondering.load() && g_time_manager.on_iter(depth, best, score))
                            g_stop = true;
                    };

                    SearchResult result = search(
                        b_copy, root_depth,
                        rep.data(), static_cast<int>(rep.size()),
                        searchmoves,
                        on_iter, false);

                    *timer_active = false;
                    g_stop        = true;
                    timer.join();

                    if (!is_legal_root_move(b_copy, result.best_move))
                        result.best_move = first_legal_move(b_copy);

                    while (pondering && g_pondering.load() && !g_stop.load())
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));

                    std::string ponder_str = ponder_suffix(b_copy, result.best_move);

                    std::cout << "bestmove " << move_to_uci(result.best_move)
                              << ponder_str << "\n";
                    std::cout.flush();
                    active_tt = &TT;
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

            U64  total_nodes = 0;
            auto bench_start = std::chrono::steady_clock::now();
            std::cout << "Running bench...\n";

            for (const auto &fen : bench_fens) {
                Board bench_b;
                set_from_fen(bench_b, fen);
                std::vector<Move> sm;
                SearchResult res = search(bench_b, 10, nullptr, 0, sm);
                total_nodes += res.nodes;
            }

            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - bench_start).count();
            if (ms == 0) ms = 1;

            std::cout << "\n===========================\n"
                      << "Nodes: " << total_nodes << "\n"
                      << "Time : " << ms << " ms\n"
                      << "NPS  : " << (total_nodes * 1000) / ms << "\n"
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
