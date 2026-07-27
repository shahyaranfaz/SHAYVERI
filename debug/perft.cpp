#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move_gen.h"
#include "types.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using SHAYVERI::Board;
using SHAYVERI::MoveList;
using SHAYVERI::Undo;
using SHAYVERI::U64;
using SHAYVERI::generate_legal_moves;
using SHAYVERI::init_attacks;
using SHAYVERI::make_legal_move;
using SHAYVERI::set_from_fen;
using SHAYVERI::set_startpos;
using SHAYVERI::unmake_move;

U64 perft(Board &b, int depth) {
    if (depth == 0) return 1;
    U64 count = 0;
    MoveList moves = generate_legal_moves(b);
    for (int i = 0; i < moves.count; ++i) {
        Undo u;
        make_legal_move(b, moves.moves[i], u);
        count += perft(b, depth - 1);
        unmake_move(b, moves.moves[i], u);
    }
    return count;
}

static void usage() {
    std::cout
        << "Usage:\n"
        << "  perft startpos <depth>\n"
        << "  perft fen \"<fen>\" <depth>\n"
        << "  perft suite   # run built-in regression suite\n";
}

struct PerftCase {
    const char *name;
    const char *fen;
    int depth;
    U64 expected;
    const char *source;
};

static int run_suite() {
    // Source: Chessprogramming Wiki perft results (includes Kiwipete and standard regression set).
    const std::vector<PerftCase> cases = {
        {"Start Position depth 1", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Start Position depth 2", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Start Position depth 3", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Start Position depth 4", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Start Position depth 5", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Kiwipete depth 1", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Kiwipete depth 2", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Kiwipete depth 3", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Kiwipete depth 4", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Position 3 depth 1", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 1, 14ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Position 3 depth 2", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2, 191ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Position 3 depth 3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Position 3 depth 4", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4, 43238ULL, "https://www.chessprogramming.org/Perft_Results"},
        {"Position 3 depth 5", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624ULL, "https://www.chessprogramming.org/Perft_Results"},
    };

    int failures = 0;
    for (const auto &tc : cases) {
        Board b;
        if (!set_from_fen(b, tc.fen)) {
            std::cerr << "[FAIL] " << tc.name << " - could not parse FEN\n";
            ++failures;
            continue;
        }
        const U64 actual = perft(b, tc.depth);
        if (actual != tc.expected) {
            std::cerr << "[FAIL] " << tc.name << ": expected " << tc.expected << ", got " << actual
                      << " | source: " << tc.source << "\n";
            ++failures;
        } else {
            std::cout << "[PASS] " << tc.name << " (" << actual << ")\n";
        }
    }

    if (failures == 0) {
        std::cout << "Suite passed: " << cases.size() << "/" << cases.size() << "\n";
        return 0;
    }
    std::cerr << "Suite failed: " << failures << "/" << cases.size() << " failed\n";
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }

    Board b;
    std::string mode = argv[1];
    int depth = 0;
    init_attacks();

    if (mode == "suite") return (argc == 2) ? run_suite() : (usage(), 1);
    if (argc < 3) { usage(); return 1; }

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
