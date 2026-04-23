#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "search.h"

#include <iostream>
#include <sstream>
#include <string>

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

        } else if (token == "position") {
            std::string pos;
            iss >> pos;
            if (pos == "startpos") {
                set_startpos(b);
            } else if (pos == "fen") {
                std::string fen, part;
                for (int i = 0; i < 6 && iss >> part; ++i)
                    fen += (i ? " " : "") + part;
                set_from_fen(b, fen);
            }
            std::string moves_token;
            if (iss >> moves_token && moves_token == "moves") {
                std::string mv;
                while (iss >> mv) {
                    Move m = uci_to_move(b, mv);
                    if (m != MOVE_NONE) {
                        Undo u;
                        make_move(b, m, u);
                    }
                }
            }

        } else if (token == "go") {
            SearchResult result = search(b, 6);
            if (result.best_move != MOVE_NONE)
                std::cout << "bestmove " << move_to_uci(result.best_move) << "\n";
            else
                std::cout << "bestmove 0000\n";

        } else if (token == "quit") {
            break;
        }

        std::cout.flush();
    }
    return 0;
}