#include "board.h"
#include "make.h"
#include "attacks.h"
#include "move_gen.h"

#include <iostream>
#include <string>

U64 perft(Board &b, int depth) {
    if (depth == 0) return 1;
    U64 count = 0;
    MoveList moves = generate_legal_moves(b);
    for (int i = 0; i < moves.count; ++i) {
        Undo u;
        if (!make_move(b, moves.moves[i], u)) continue;
        count += perft(b, depth - 1);
        unmake_move(b, moves.moves[i], u);
    }
    return count;
}

static void usage() {
    std::cout
        << "Usage:\n"
        << "  perft startpos <depth>\n"
        << "  perft fen \"<fen>\" <depth>\n";
}

int main(int argc, char **argv) {
    if (argc < 3) { usage(); return 1; }

    Board b;
    std::string mode = argv[1];
    int depth = 0;
    init_attacks();

    if (mode == "startpos") {
        if (argc != 3) { usage(); return 1; }
        try {
            depth = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid depth\n";
            return 1;
        }
        if (!set_startpos(b)) {
            std::cerr << "Failed to set startpos\n";
            return 1;
        }
    } else if (mode == "fen") {
        if (argc != 4) { usage(); return 1; }
        std::string fen = argv[2];
        try {
            depth = std::stoi(argv[3]);
        } catch (...) {
            std::cerr << "Invalid depth\n";
            return 1;
        }
        if (!set_from_fen(b, fen)) {
            std::cerr << "Bad FEN\n";
            return 1;
        }
    } else {
        usage();
        return 1;
    }

    auto nodes = perft(b, depth);
    std::cout << "perft(" << depth << ") = " << nodes << "\n";
    return 0;
}