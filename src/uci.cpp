#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "opening_book.h"
#include "search.h"
#include "time_manager.h"
#include "tt.h"
#include "zobrist.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "evaluate.h"

// Global options (written by setoption, read by go)
static int  g_num_threads    = 1;
static int  g_move_overhead  = 10;   // ms
static bool g_ponder         = false;

// All active search threads
// 0 is main thread (bestmove reporter)
static std::vector<std::thread> smp_threads;

static void stop_search() {
    g_stop = true;
    for (auto& t : smp_threads)
        if (t.joinable()) t.join();
    smp_threads.clear();
}

static const BookEntry *probe_book(U64 zobrist_key) {
    // Binary search for the key in our sorted array
    auto it = std::lower_bound(OPENING_BOOK, OPENING_BOOK + OPENING_BOOK_SIZE, zobrist_key,
                               [](const BookEntry &entry, U64 key) {
                                   return entry.key < key;
                               });

    // Check if we found the exact key
    if (it != OPENING_BOOK + OPENING_BOOK_SIZE && it->key == zobrist_key)
        return it;

    return nullptr;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Zobrist::init();
    init_attacks();
    TT.resize(64);

    Board b;
    set_startpos(b);

    std::vector<std::string> move_history;
    std::vector<U64>         hash_history;
    hash_history.push_back(b.hash);

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "uci") {
            std::cout << "id name ShayBot\n";
            std::cout << "id author Shahyar\n";
            std::cout << "option name Hash type spin default 64 min 1 max 32768\n";
            std::cout << "option name Clear Hash type button\n";
            std::cout << "option name Threads type spin default 1 min 1 max 512\n";
            std::cout << "option name Ponder type check default false\n";
            std::cout << "option name Move Overhead type spin default 10 min 0 max 5000\n";
            std::cout << "uciok\n";
        } else if (token == "setoption") {
            std::string skip, name;
            iss >> skip; // Skip the word "name"
            std::string opt_name;
            while (iss >> name && name != "value") {
                if (!opt_name.empty()) opt_name += " ";
                opt_name += name;
            }

            // Read the value token (may be empty for buttons)
            std::string value;
            std::getline(iss, value);
            if (!value.empty() && value.front() == ' ')
                value.erase(value.begin());

            if (opt_name == "Hash") {
                int mb = std::stoi(value);
                TT.resize(mb);
            } else if (opt_name == "Clear Hash") {
                TT.clear();
            } else if (opt_name == "Threads") {
                g_num_threads = std::max(1, std::stoi(value));
            } else if (opt_name == "Move Overhead") {
                g_move_overhead = std::max(0, std::stoi(value));
            } else if (opt_name == "Ponder") {
                g_ponder = (value == "true");
            }
        } else if (token == "isready") {
            std::cout << "readyok\n";
        } else if (token == "ucinewgame") {
            stop_search();
            set_startpos(b);
            hash_history.clear();
            hash_history.push_back(b.hash);
            move_history.clear();
            TT.clear();
        } else if (token == "position") {
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
                        }
                    }
                }
            }
        } else if (token == "go") {

            // 1. Book probe
            const BookEntry* entry = probe_book(b.hash);
            if (entry) {
                Move m = uci_to_move(b, entry->move);
                if (m != MOVE_NONE) {
                    stop_search(); // join any previous thread
                    g_stop = false;
                    node_count = 0;

                    int book_eval_cp = static_cast<int>(entry->evaluation * 100.0f);
                    if (b.side_to_move == BLACK) book_eval_cp = -book_eval_cp;

                    std::cout << "info depth 8 score cp " << book_eval_cp
                              << " pv " << entry->move << " (book)\n";
                    std::cout << "bestmove " << entry->move << "\n";
                    std::cout.flush();
                    continue;
                }
            }

            // 2. Stop any existing search
            stop_search();
            g_stop = false;
            node_count = 0;

            // 3. Parse go parameters
            TimeControl tc;
            tc.side          = b.side_to_move;
            tc.move_overhead = g_move_overhead;

            std::vector<Move> searchmoves;
            std::string tok;
            while (iss >> tok) {
                if      (tok == "infinite")    tc.infinite  = true;
                else if (tok == "wtime")       iss >> tc.wtime;
                else if (tok == "btime")       iss >> tc.btime;
                else if (tok == "winc")        iss >> tc.winc;
                else if (tok == "binc")        iss >> tc.binc;
                else if (tok == "movetime")    iss >> tc.movetime;
                else if (tok == "searchmoves") {
                    std::string sm_str;
                    while (iss >> sm_str) {
                        Move sm = uci_to_move(b, sm_str);
                        if (sm != MOVE_NONE) searchmoves.push_back(sm);
                    }
                }
            }

            // 4. Initialise time manager
            g_time_manager.init(tc);

            // 5. Build repetition history
            int start = static_cast<int>(hash_history.size()) - 1 - b.half_move;
            if (start < 0) start = 0;
            std::vector<U64> rep(hash_history.begin() + start, hash_history.end());

            // 6. Capture state for threads
            Board b_copy       = b;
            int num_threads  = g_num_threads;
            I64 hard_ms = g_time_manager.hard_ms();

            // 7. Launch Lazy SMP helper threads
            // Helpers run the same search on their own Board copy,
            // sharing the TT (lockless — benign races are acceptable).
            // They use no IterCallback and run until g_stop is set.
            // Thread index t > 0: alternate depth ±1 to diversify the
            // tree they explore relative to the main thread.
            for (int t = 1; t < num_threads; ++t) {
                smp_threads.push_back(
                    std::thread([b_copy, rep, t, searchmoves]() mutable {
                        int helper_depth = 64 + (t % 2 == 0 ? 1 : -1);
                        std::vector<Move> no_sm; // helpers don't restrict moves
                        search(b_copy, helper_depth,
                               rep.data(), static_cast<int>(rep.size()),
                               no_sm, nullptr, true);
                        // result is intentionally discarded
                    })
                );
            }

            // 8. Main search thread
            // Uses IterCallback for dynamic time management:
            //   • After each completed depth, check soft limit
            //   • A hard-limit timer thread fires independently
            smp_threads.insert(
                smp_threads.begin(),
                std::thread([b_copy, rep, searchmoves, hard_ms]() mutable {

                    // Hard-limit timer: fires when the absolute deadline
                    // is reached regardless of move stability.
                    auto timer_active = std::make_shared<std::atomic<bool>>(true);
                    std::thread timer([hard_ms, timer_active]() {
                        // Sleep in small increments so we can exit early
                        // if the search finishes before the hard limit.
                        const I64 slice = 5; // ms
                        I64 slept = 0;
                        while (*timer_active && slept < hard_ms) {
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(slice));
                            slept += slice;
                        }
                        if (*timer_active) g_stop = true;
                    });

                    // Per-iteration callback: dynamic soft-limit logic
                    IterCallback on_iter = [](int depth, Move best, int score,
                                             U64 /*nodes*/, I64 /*ms*/) {
                        if (g_time_manager.on_iter(depth, best, score))
                            g_stop = true;
                    };

                    SearchResult result = search(
                        b_copy, 64,
                        rep.data(), static_cast<int>(rep.size()),
                        searchmoves,
                        on_iter, false);

                    // Disarm timer
                    *timer_active = false;
                    g_stop        = true;
                    timer.join();

                    // Fallback: if search returned MOVE_NONE pick first legal
                    if (result.best_move == MOVE_NONE) {
                        MoveList legal = generate_legal_moves(b_copy);
                        if (legal.count > 0)
                            result.best_move = legal.moves[0];
                    }

                    std::cout << "bestmove " << move_to_uci(result.best_move) << "\n";
                    std::cout.flush();
                })
            );
        }

        // --------------------------------------------------------
        else if (token == "bench") {
            std::vector<std::string> bench_fens = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 5 20"
            };

            U64 total_nodes = 0;
            std::cout << "Running bench...\n";
            auto bench_start = std::chrono::steady_clock::now();

            for (const auto& fen : bench_fens) {
                Board bench_b;
                set_from_fen(bench_b, fen);
                std::vector<Move> sm;
                SearchResult res = search(bench_b, 10, nullptr, 0, sm);
                total_nodes += res.nodes;
            }

            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - bench_start).count();
            if (ms == 0) ms = 1;

            std::cout << "\n===========================\n";
            std::cout << "Nodes: " << total_nodes << "\n";
            std::cout << "Time : " << ms << " ms\n";
            std::cout << "NPS  : " << (total_nodes * 1000) / ms << "\n";
            std::cout << "===========================\n";
        } else if (token == "stop") {
            stop_search();
        } else if (token == "quit") {
            stop_search();
            break;
        }
        std::cout.flush();
    }
    return 0;
}