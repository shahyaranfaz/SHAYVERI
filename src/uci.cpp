#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "opening_book.h"
#include "search.h"
#include "tt.h"
#include "zobrist.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "evaluate.h"

static std::thread search_thread;

static void stop_search() {
    g_stop = true;
    if (search_thread.joinable())
        search_thread.join();
}

// opening book

static U64 fnv64(const std::string& s) {
    U64 h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

static std::string probe_book(const std::vector<std::string>& history) {
    std::string prefix;
    for (int i = 0; i < (int)history.size(); ++i) {
        if (i > 0) prefix += ' ';
        prefix += history[i];
    }
    U64 key = fnv64(prefix);
    for (int i = 0; i < OPENING_BOOK_SIZE; ++i) {
        if (OPENING_BOOK[i].key == key)
            return std::string(OPENING_BOOK[i].move);
    }
    return "";
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

    std::vector<U64> hash_history;
    hash_history.clear();
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
            std::cout << "option name Ponder type check default false\n";
            std::cout << "uciok\n";

        } else if (token == "setoption") {
            std::string skip, name;
            iss >> skip; // Skip the word "name"
            std::string opt_name;
            while (iss >> name && name != "value") {
                if (!opt_name.empty()) opt_name += " ";
                opt_name += name;
            }
            if (opt_name == "Hash") {
                int mb;
                if (iss >> mb) TT.resize(mb);
            } else if (opt_name == "Clear Hash") {
                TT.clear();
            }

        } else if (token == "bench") {
            std::vector<std::string> bench_fens = {
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 5 20"
            };
            U64 total_nodes = 0;
            std::cout << "Running bench..." << std::endl;
            auto start_time = std::chrono::steady_clock::now();

            for (const auto& fen : bench_fens) {
                Board bench_b;
                set_from_fen(bench_b, fen);
                std::vector<Move> sm; // empty searchmoves
                SearchResult res = search(bench_b, 10, nullptr, 0, sm); // fixed depth 10
                total_nodes += res.nodes;
            }

            auto end_time = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            if (ms == 0) ms = 1;
            std::cout << "\n===========================\n";
            std::cout << "Nodes: " << total_nodes << "\n";
            std::cout << "Time : " << ms << " ms\n";
            std::cout << "NPS  : " << (total_nodes * 1000) / ms << "\n";
            std::cout << "===========================\n";

        } else if (token == "isready") {
            std::cout << "readyok\n";

        } else if (token == "ucinewgame") {
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
            std::string book_move = probe_book(move_history);
            bool use_book = false;
            if (!book_move.empty()) {
                Move m = uci_to_move(b, book_move);
                if (m != MOVE_NONE) {
                    Undo book_undo;
                    make_move(b, m, book_undo);
                    int book_eval = -evaluate(b);  // Evaluate AFTER the move, from the previous side's perspective
                    unmake_move(b, m, book_undo);
                    std::cout << "info depth 1 score cp " << book_eval << " pv " << book_move << "\n";
                    std::cout << "bestmove " << book_move << "\n";
                    use_book = true;
                }
            }
            if (!use_book) {
                stop_search();

                bool infinite = false;
                int wtime = 0, btime = 0, winc = 0, binc = 0, movetime = 0;
                std::vector<Move> searchmoves;
                std::string tok;
                while (iss >> tok) {
                    if (tok == "infinite") infinite = true;
                    else if (tok == "wtime") iss >> wtime;
                    else if (tok == "btime") iss >> btime;
                    else if (tok == "winc") iss >> winc;
                    else if (tok == "binc") iss >> binc;
                    else if (tok == "movetime") iss >> movetime;
                    else if (tok == "searchmoves") {
                        std::string sm_str;
                        // consume all the remaining tokens as moves until end of stream
                        while (iss >> sm_str) {
                            Move sm = uci_to_move(b, sm_str);
                            if (sm != MOVE_NONE) searchmoves.push_back(sm);
                        }
                    }
                }

            int my_time = (b.side_to_move == WHITE) ? wtime : btime;
            int my_inc  = (b.side_to_move == WHITE) ? winc  : binc;
            int alloc = infinite ? 60000 : (movetime > 0 ? movetime : my_time / 20 + my_inc / 2);
            if (!infinite && movetime == 0 && alloc >= my_time) alloc = my_time - 50;
            // 50ms safety buffer to prevent flagging
            if (alloc < 10) alloc = 10;

            int start = (int)hash_history.size() - 1 - b.half_move;
            if (start < 0) start = 0;

            std::vector<U64> rep(hash_history.begin() + start, hash_history.end());

            Board b_copy = b;
            search_thread = std::thread([b_copy, alloc, rep, searchmoves]() mutable {
                auto timer_active = std::make_shared<std::atomic<bool>>(true);
                std::thread timer([alloc, timer_active]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(alloc));
                    if (*timer_active) g_stop = true;
                });

                SearchResult result = search(b_copy, 64, rep.data(), (int)rep.size(), searchmoves);
                *timer_active = false;
                g_stop = true;
                timer.join();

                if (result.best_move == MOVE_NONE) {
                    MoveList legal = generate_legal_moves(b_copy);
                    if (legal.count > 0) result.best_move = legal.moves[0];
                }

                std::cout << "bestmove " << move_to_uci(result.best_move) << "\n";
                std::cout.flush();
            });
        }
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
