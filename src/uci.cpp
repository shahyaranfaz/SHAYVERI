#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "opening_book.h"
#include "search.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static std::thread search_thread;

static void stop_search() {
    g_stop = true;
    if (search_thread.joinable())
        search_thread.join();
}

// opening book

static uint64_t fnv64(const std::string& s) {
    uint64_t h = 14695981039346656037ULL;
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
    uint64_t key = fnv64(prefix);
    for (int i = 0; i < OPENING_BOOK_SIZE; ++i) {
        if (OPENING_BOOK[i].key == key)
            return std::string(OPENING_BOOK[i].move);
    }
    return "";
}

static std::string move_to_uci(Move m) {
    auto sq_to_str = [](Square s) -> std::string {
        std::string r;
        r += char('a' + get_file(s));
        r += char('1' + get_rank(s));
        return r;
    };
    std::string s = sq_to_str(move_from(m)) + sq_to_str(move_to(m));
    PieceType promo = move_promo(m);
    if (promo != NONE_PTYPE) {
        const char* promos = "?pnbrq";
        s += promos[promo];
    }
    return s;
}

static Move uci_to_move(Board& b, const std::string& uci) {
    if (uci.size() < 4) return MOVE_NONE;
    File ff = File(uci[0] - 'a');
    Rank fr = Rank(uci[1] - '1');
    File tf = File(uci[2] - 'a');
    Rank tr = Rank(uci[3] - '1');
    Square from = make_square(ff, fr);
    Square to = make_square(tf, tr);

    PieceType promo = NONE_PTYPE;
    if (uci.size() == 5) {
        switch (uci[4]) {
            case 'n': promo = KNIGHT; break;
            case 'b': promo = BISHOP; break;
            case 'r': promo = ROOK; break;
            case 'q': promo = QUEEN; break;
            default: break;
        }
    }

    MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i) {
        Move m = legal.moves[i];
        if (move_from(m) == from && move_to(m) == to && move_promo(m) == promo)
            return m;
    }
    return MOVE_NONE;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Board b;
    set_startpos(b);
    std::vector<std::string> move_history;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "uci") {
            std::cout << "id name ShayBot\n";
            std::cout << "id author Shahyar\n";
            std::cout << "uciok\n";

        } else if (token == "isready") {
            std::cout << "readyok\n";

        } else if (token == "ucinewgame") {
            set_startpos(b);
            move_history.clear();

        } else if (token == "position") {
            std::string pos;
            iss >> pos;
            move_history.clear();
            if (pos == "startpos") {
                set_startpos(b);
            } else if (pos == "fen") {
                std::string fen, part;
                for (int i = 0; i < 6 && iss >> part; ++i)
                    fen += (i ? " " : "") + part;
                if (!set_from_fen(b, fen))
                    set_startpos(b);
            }
            std::string moves_token;
            if (iss >> moves_token && moves_token == "moves") {
                std::string mv;
                while (iss >> mv) {
                    Move m = uci_to_move(b, mv);
                    if (m != MOVE_NONE) {
                        Undo u;
                        make_move(b, m, u);
                        move_history.push_back(mv);
                    }
                }
            }

        } else if (token == "go") {
            std::string book_move = probe_book(move_history);
            if (!book_move.empty()) {
                std::cout << "bestmove " << book_move << "\n";
            } else {
                stop_search(); // stop any existing search

                // parse wtime/btime/winc/binc/infinite
                bool infinite = false;
                int wtime = 0, btime = 0, winc = 0, binc = 0, movetime = 0;
                std::string tok;
                while (iss >> tok) {
                    if (tok == "infinite") infinite = true;
                    else if (tok == "wtime") iss >> wtime;
                    else if (tok == "btime") iss >> btime;
                    else if (tok == "winc") iss >> winc;
                    else if (tok == "binc") iss >> binc;
                    else if (tok == "movetime") iss >> movetime;
                }

                // compute time to use in ms
                int my_time = (b.side_to_move == WHITE) ? wtime : btime;
                int my_inc  = (b.side_to_move == WHITE) ? winc  : binc;
                int alloc = infinite ? 60000 : (movetime > 0 ? movetime : my_time / 20 + my_inc / 2);
                if (alloc < 10) alloc = 10;

                Board b_copy = b;
                search_thread = std::thread([b_copy, alloc]() mutable {
                    // timer thread to set stop flag
                    auto timer_active = std::make_shared<std::atomic<bool>>(true);
                    std::thread timer([alloc, timer_active]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(alloc));
                        if (*timer_active) g_stop = true;
                    });

                    SearchResult result = search(b_copy, 64);
                    *timer_active = false;
                    g_stop = true; // make timer exit cleanly if still sleeping
                    timer.join();

                    std::cout << "info depth " << result.depth
                              << " score cp " << result.score
                              << " nodes " << result.nodes << "\n";
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