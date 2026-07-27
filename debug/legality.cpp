#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "zobrist.h"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using SHAYVERI::Board;
using SHAYVERI::File;
using SHAYVERI::Move;
using SHAYVERI::MoveList;
using SHAYVERI::PieceType;
using SHAYVERI::Rank;
using SHAYVERI::Square;
using SHAYVERI::SQ_NONE;
using SHAYVERI::generate_legal_moves;
using SHAYVERI::generate_legal_moves_checked;
using SHAYVERI::init_attacks;
using SHAYVERI::make_generated_move;
using SHAYVERI::move_from;
using SHAYVERI::move_promo;
using SHAYVERI::move_to;
using SHAYVERI::set_from_fen;
using SHAYVERI::make_square;
using SHAYVERI::BISHOP;
using SHAYVERI::KNIGHT;
using SHAYVERI::NONE_PTYPE;
using SHAYVERI::QUEEN;
using SHAYVERI::ROOK;
using SHAYVERI::Undo;

struct MoveLegalityCase {
    const char *name;
    const char *fen;
    const char *uci_move;
    bool expected_legal;
    const char *source;
};

static bool parse_uci_move(const std::string &uci, Square &from, Square &to, PieceType &promo) {
    if (uci.size() < 4 || uci.size() > 5) return false;
    if (uci[0] < 'a' || uci[0] > 'h' || uci[2] < 'a' || uci[2] > 'h') return false;
    if (uci[1] < '1' || uci[1] > '8' || uci[3] < '1' || uci[3] > '8') return false;

    from = make_square(File(uci[0] - 'a'), Rank(uci[1] - '1'));
    to   = make_square(File(uci[2] - 'a'), Rank(uci[3] - '1'));
    promo = NONE_PTYPE;
    if (uci.size() == 5) {
        switch (uci[4]) {
            case 'n': promo = KNIGHT; break;
            case 'b': promo = BISHOP; break;
            case 'r': promo = ROOK; break;
            case 'q': promo = QUEEN; break;
            default: return false;
        }
    }
    return true;
}

static bool contains_legal_move(Board &b, const std::string &uci) {
    Square from = SQ_NONE;
    Square to = SQ_NONE;
    PieceType promo = NONE_PTYPE;
    if (!parse_uci_move(uci, from, to, promo)) return false;

    MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i) {
        const Move m = legal.moves[i];
        if (move_from(m) == from && move_to(m) == to && move_promo(m) == promo) {
            return true;
        }
    }
    return false;
}

static std::vector<Move> sorted_moves(const MoveList &moves) {
    std::vector<Move> result(moves.moves, moves.moves + moves.count);
    std::sort(result.begin(), result.end());
    return result;
}

static bool generators_match(Board &b) {
    const MoveList direct = generate_legal_moves(b);
    const MoveList checked = generate_legal_moves_checked(b);
    if (direct.count != checked.count) return false;
    for (int i = 0; i < direct.count; ++i)
        if (direct.moves[i] != checked.moves[i]) return false;
    return true;
}

static void print_generator_difference(Board &b) {
    const std::vector<Move> direct =
        sorted_moves(generate_legal_moves(b));
    const std::vector<Move> checked =
        sorted_moves(generate_legal_moves_checked(b));
    std::cerr << "  FEN: " << SHAYVERI::get_board_fen(b) << "\n  direct-only:";
    for (Move move : direct)
        if (!std::binary_search(checked.begin(), checked.end(), move))
            std::cerr << " " << move;
    std::cerr << "\n  checked-only:";
    for (Move move : checked)
        if (!std::binary_search(direct.begin(), direct.end(), move))
            std::cerr << " " << move;
    std::cerr << "\n";
}

int main() {
    // Source: niklasf/python-chess test suite (en passant legality edge cases).
    const std::vector<MoveLegalityCase> cases = {
        {"EP legal evasion", "8/8/8/2k5/2pP4/8/4K3/8 b - d3 0 1", "c4d3", true, "https://github.com/niklasf/python-chess/blob/master/test.py"},
        {"EP illegal horizontal skewer", "8/8/8/r2Pp2K/8/8/4k3/8 w - e6 0 1", "d5e6", false, "https://github.com/niklasf/python-chess/blob/master/test.py"},
        {"EP legal diagonal case", "2b1r2r/8/5P1k/2p1pP2/5R1P/6PK/4q3/4R3 w - e6 0 1", "f5e6", true, "https://github.com/niklasf/python-chess/blob/master/test.py"},
        {"EP illegal diagonal skewer", "8/8/8/5k2/4Pp2/8/2B5/4K3 b - e3 0 1", "f4e3", false, "https://github.com/niklasf/python-chess/blob/master/test.py"},
        {"EP illegal diagonal non-evasion", "8/8/8/7B/6Pp/8/4k2K/3r4 b - g3 0 1", "h4g3", false, "https://github.com/niklasf/python-chess/blob/master/test.py"},
        {"EP illegal file pin", "8/5K2/8/3k4/3pP3/8/8/3R4 b - e3 0 1", "d4e3", false, "https://github.com/niklasf/python-chess/blob/master/test.py"},
    };

    init_attacks();
    SHAYVERI::Zobrist::init();

    int failures = 0;
    for (const auto &tc : cases) {
        Board b;
        if (!set_from_fen(b, tc.fen)) {
            std::cerr << "[FAIL] " << tc.name << ": invalid FEN in test case\n";
            ++failures;
            continue;
        }

        const bool actual = contains_legal_move(b, tc.uci_move);
        if (actual != tc.expected_legal) {
            std::cerr << "[FAIL] " << tc.name
                      << ": move " << tc.uci_move
                      << " expected legal=" << (tc.expected_legal ? "true" : "false")
                      << " got " << (actual ? "true" : "false")
                      << " | source: " << tc.source << "\n";
            ++failures;
        } else {
            std::cout << "[PASS] " << tc.name << "\n";
        }
    }

    const std::vector<const char *> equality_fens = {
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        "4k3/8/8/8/8/2b5/3r4/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/4r3/4K3 w - - 0 1",
        "4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1",
        "4k3/P6P/8/8/8/8/p6p/4K3 w - - 0 1",
    };
    for (const char *fen : equality_fens) {
        Board b;
        if (!set_from_fen(b, fen) || !generators_match(b)) {
            std::cerr << "[FAIL] direct/checked mismatch for FEN: "
                      << fen << "\n";
            if (b.is_consistent()) print_generator_difference(b);
            ++failures;
        }
    }

    std::mt19937_64 rng(0x6C6567616C4D6F76ULL);
    int randomized_positions = 0;
    for (int game = 0; game < 128; ++game) {
        Board b;
        if (!set_from_fen(
                b,
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/"
                "RNBQKBNR w KQkq - 0 1")) {
            ++failures;
            break;
        }
        for (int ply = 0; ply < 96; ++ply) {
            const MoveList direct = generate_legal_moves(b);
            const MoveList checked = generate_legal_moves_checked(b);
            ++randomized_positions;
            bool equal = direct.count == checked.count;
            for (int i = 0; equal && i < direct.count; ++i)
                equal = direct.moves[i] == checked.moves[i];
            if (!equal) {
                std::cerr << "[FAIL] randomized direct/checked mismatch"
                          << " game=" << game << " ply=" << ply << "\n";
                print_generator_difference(b);
                ++failures;
                break;
            }
            if (direct.count == 0) break;

            const Move move =
                direct.moves[static_cast<int>(rng() % direct.count)];
            Undo undo;
            if (!make_generated_move(b, move, undo)) {
                std::cerr << "[FAIL] direct generator emitted illegal move"
                          << " game=" << game << " ply=" << ply << "\n";
                ++failures;
                break;
            }
        }
    }

    if (failures == 0) {
        std::cout << "Legality suite passed: " << cases.size() << "/"
                  << cases.size() << ", direct/checked equality across "
                  << randomized_positions << " randomized positions\n";
        return 0;
    }

    std::cerr << "Legality suite failed: " << failures << "/" << cases.size() << " failed\n";
    return 2;
}
