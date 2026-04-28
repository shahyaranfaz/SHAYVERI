#include "attacks.h"
#include "board.h"
#include "evaluate.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "opening_book.h"
#include "search.h"
#include "time_manager.h"
#include "tt.h"
#include "tune.h"
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

namespace ShayBot {

// ----------------------------------------------------------------
// UCI options (written by setoption, read by go / search)
// ----------------------------------------------------------------
static int  g_num_threads   = 1;
static int  g_move_overhead = 10;   // ms
static bool g_ponder        = false;
static int  g_multi_pv      = 1;
static int  g_min_think_ms  = 0;
static int  g_nodes_time    = 0;    // 0 = disabled
static bool g_show_wdl      = false;
static bool g_analyse_mode  = false;
static bool g_chess960      = false;
static std::string g_opponent;

// Ponder state
static Move g_ponder_move      = MOVE_NONE;  // move we are pondering on
static bool g_pondering        = false;       // currently in ponder search
static std::mutex g_ponder_mtx;

// All active search threads (index 0 = main thread)
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

} // namespace ShayBot

// main() must be at global scope (C++ entry point)
int main() {
    using namespace ShayBot;

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

        // --------------------------------------------------------
        if (token == "uci") {
            std::cout
                << "id name ShayBot\n"
                << "id author Shahyar\n"
                << "option name Hash type spin default 64 min 1 max 32768\n"
                << "option name Clear Hash type button\n"
                << "option name Threads type spin default 1 min 1 max 512\n"
                << "option name Ponder type check default false\n"
                << "option name MultiPV type spin default 1 min 1 max 500\n"
                << "option name Minimum Thinking Time type spin default 0 min 0 max 5000\n"
                << "option name nodestime type spin default 0 min 0 max 10000\n"
                << "option name Move Overhead type spin default 10 min 0 max 5000\n"
                << "option name UCI_ShowWDL type check default false\n"
                << "option name UCI_AnalyseMode type check default false\n"
                << "option name UCI_Chess960 type check default false\n"
                << "option name UCI_Opponent type string default \"\"\n";

            for (auto const& [name, opt] : ShayBot::Tune::tuning_registry) {
                if (opt.type == ShayBot::Tune::TuningOption::INT)
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

        // --------------------------------------------------------
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
                TT.resize(std::stoi(value));
            else if (opt_name == "Clear Hash")
                TT.clear();
            else if (opt_name == "Threads")
                g_num_threads = std::max(1, std::stoi(value));
            else if (opt_name == "Ponder")
                g_ponder = (value == "true");
            else if (opt_name == "MultiPV")
                g_multi_pv = std::max(1, std::stoi(value));
            else if (opt_name == "Minimum Thinking Time")
                g_min_think_ms = std::max(0, std::stoi(value));
            else if (opt_name == "nodestime")
                g_nodes_time = std::max(0, std::stoi(value));
            else if (opt_name == "Move Overhead")
                g_move_overhead = std::max(0, std::stoi(value));
            else if (opt_name == "UCI_ShowWDL")
                g_show_wdl = (value == "true");
            else if (opt_name == "UCI_AnalyseMode")
                g_analyse_mode = (value == "true");
            else if (opt_name == "UCI_Chess960")
                g_chess960 = (value == "true");
            else if (opt_name == "UCI_Opponent")
                g_opponent = value;
            else
                Tune::handle_setoption(opt_name, value);
        }

        // --------------------------------------------------------
        else if (token == "isready") {
            std::cout << "readyok\n";
        }

        // --------------------------------------------------------
        else if (token == "ucinewgame") {
            stop_search();
            set_startpos(b);
            hash_history.clear();
            hash_history.push_back(b.hash);
            move_history.clear();
            TT.clear();
            g_ponder_move = MOVE_NONE;
            g_pondering   = false;
        }

        // --------------------------------------------------------
        else if (token == "position") {
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
        }

        // --------------------------------------------------------
        else if (token == "go") {
            // Book probe (only for normal searches, not ponder)
            bool is_ponder_search = false;
            {
                std::istringstream check(line);
                std::string t;
                while (check >> t)
                    if (t == "ponder") { is_ponder_search = true; break; }
            }

            if (!is_ponder_search) {
                const BookEntry *entry = probe_book(b.hash);
                if (entry) {
                    Move m = uci_to_move(b, entry->move);
                    if (m != MOVE_NONE) {
                        stop_search();
                        g_stop     = false;
                        node_count = 0;

                        int book_eval_cp = static_cast<int>(entry->evaluation * 100.0f);
                        if (b.side_to_move == BLACK) book_eval_cp = -book_eval_cp;

                        // Try to find a ponder move from TT
                        std::string ponder_str;
                        {
                            Board tmp = b;
                            Undo u;
                            if (make_move(tmp, m, u)) {
                                if (const TTEntry *pe = TT.probe(tmp.hash)) {
                                    if (pe->best != MOVE_NONE)
                                        ponder_str = " ponder " + move_to_uci(pe->best);
                                }
                            }
                        }

                        std::cout << "info depth 8 score cp " << book_eval_cp
                                  << " pv " << entry->move << " \n"
                                  << "bestmove " << entry->move
                                  << ponder_str << "\n";
                        std::cout.flush();
                        continue;
                    }
                }
            }

            stop_search();
            g_stop     = false;
            node_count = 0;

            // Parse go parameters
            TimeControl tc;
            tc.side          = b.side_to_move;
            tc.move_overhead = g_move_overhead;
            tc.min_think_ms  = g_min_think_ms;

            std::vector<Move> searchmoves;
            {
                std::istringstream go_iss(line);
                std::string tok;
                go_iss >> tok; // consume "go"
                while (go_iss >> tok) {
                    if      (tok == "infinite")  tc.infinite  = true;
                    else if (tok == "ponder")    { /* handled via is_ponder_search */ }
                    else if (tok == "wtime")     go_iss >> tc.wtime;
                    else if (tok == "btime")     go_iss >> tc.btime;
                    else if (tok == "winc")      go_iss >> tc.winc;
                    else if (tok == "binc")      go_iss >> tc.binc;
                    else if (tok == "movetime")  go_iss >> tc.movetime;
                    else if (tok == "nodes")     { I64 n; go_iss >> n; /* future: node limit */ }
                    else if (tok == "movestogo") { int m; go_iss >> m; }
                    else if (tok == "searchmoves") {
                        std::string sm_str;
                        while (go_iss >> sm_str) {
                            Move sm = uci_to_move(b, sm_str);
                            if (sm != MOVE_NONE) searchmoves.push_back(sm);
                        }
                    }
                }
            }

            // Ponder: run with infinite time; ponderhit will restart with real tc
            if (is_ponder_search) {
                tc.infinite   = true;
                g_pondering   = true;
            } else {
                g_pondering = false;
            }

            g_time_manager.init(tc);

            int   start = static_cast<int>(hash_history.size()) - 1 - b.half_move;
            if (start < 0) start = 0;
            std::vector<U64> rep(hash_history.begin() + start, hash_history.end());

            Board b_copy    = b;
            int   num_thr   = g_num_threads;
            I64   hard_ms   = g_time_manager.hard_ms();
            bool  pondering = is_ponder_search;

            // Helper threads (Lazy SMP)
            for (int t = 1; t < num_thr; ++t) {
                smp_threads.push_back(
                    std::thread([b_copy, rep, t, searchmoves]() mutable {
                        int helper_depth = 64 + (t % 2 == 0 ? 1 : -1);
                        std::vector<Move> no_sm;
                        search(b_copy, helper_depth,
                               rep.data(), static_cast<int>(rep.size()),
                               no_sm, nullptr, true);
                    })
                );
            }

            // Main search thread
            smp_threads.insert(
                smp_threads.begin(),
                std::thread([b_copy, rep, searchmoves, hard_ms, pondering]() mutable {
                    auto timer_active = std::make_shared<std::atomic<bool>>(true);

                    // Hard-limit timer (fires only for non-ponder searches)
                    std::thread timer([hard_ms, pondering, timer_active]() {
                        if (pondering) {
                            // Block until timer_active is cleared (ponderhit sets g_stop)
                            while (*timer_active)
                                std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

                    IterCallback on_iter = [pondering](int depth, Move best, int score,
                                                        U64, I64) {
                        if (!pondering && g_time_manager.on_iter(depth, best, score))
                            g_stop = true;
                    };

                    SearchResult result = search(
                        b_copy, 64,
                        rep.data(), static_cast<int>(rep.size()),
                        searchmoves,
                        on_iter, false);

                    *timer_active = false;
                    g_stop        = true;
                    timer.join();

                    if (result.best_move == MOVE_NONE) {
                        MoveList legal = generate_legal_moves(b_copy);
                        if (legal.count > 0)
                            result.best_move = legal.moves[0];
                    }

                    // Determine ponder move from TT
                    std::string ponder_str;
                    if (result.best_move != MOVE_NONE) {
                        Board tmp = b_copy;
                        Undo u;
                        if (make_move(tmp, result.best_move, u)) {
                            if (const TTEntry *pe = TT.probe(tmp.hash)) {
                                if (pe->best != MOVE_NONE) {
                                    g_ponder_move = pe->best;
                                    ponder_str = " ponder " + move_to_uci(pe->best);
                                }
                            }
                        }
                    }

                    std::cout << "bestmove " << move_to_uci(result.best_move)
                              << ponder_str << "\n";
                    std::cout.flush();
                })
            );
        }

        // --------------------------------------------------------
        else if (token == "ponderhit") {
            // GUI accepted the ponder move; restart time management with real clock
            {
                std::lock_guard<std::mutex> lk(g_ponder_mtx);
                g_pondering = false;
            }
            // Signal the running ponder search to stop; it will output bestmove.
            // The GUI will then send a new "go" with real time limits.
            g_stop = true;
        }

        // --------------------------------------------------------
        else if (token == "stop") {
            stop_search();
        }

        // --------------------------------------------------------
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

        // --------------------------------------------------------
        else if (token == "quit") {
            stop_search();
            break;
        }

        std::cout.flush();
    }
    stop_search();
    return 0;
}
