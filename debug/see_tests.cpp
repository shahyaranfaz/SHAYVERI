#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move.h"
#include "move_gen.h"
#include "move_io.h"
#include "see.h"
#include "zobrist.h"

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
using SHAYVERI::NONE_PTYPE;
using SHAYVERI::QUEEN;
using SHAYVERI::ROOK;
using SHAYVERI::BISHOP;
using SHAYVERI::KNIGHT;
using SHAYVERI::generate_legal_moves;
using SHAYVERI::init_attacks;
using SHAYVERI::make_square;
using SHAYVERI::see;
using SHAYVERI::see_ge;
using SHAYVERI::set_from_fen;

struct SeeCase {
    const char *name;
    const char *fen;
    const char *uci_move;
    int expected;
};

static bool parse_uci_move(const std::string &uci, Square &from, Square &to, PieceType &promo) {
    if (uci.size() < 4 || uci.size() > 5) return false;
    from = make_square(File(uci[0] - 'a'), Rank(uci[1] - '1'));
    to   = make_square(File(uci[2] - 'a'), Rank(uci[3] - '1'));
    promo = NONE_PTYPE;
    if (uci.size() == 5) {
        switch (uci[4]) {
            case 'q': promo = QUEEN; break;
            case 'r': promo = ROOK; break;
            case 'b': promo = BISHOP; break;
            case 'n': promo = KNIGHT; break;
            default: return false;
        }
    }
    return true;
}

static Move find_legal_move(Board &b, const std::string &uci) {
    Square from = SHAYVERI::SQ_NONE;
    Square to = SHAYVERI::SQ_NONE;
    PieceType promo = NONE_PTYPE;
    if (!parse_uci_move(uci, from, to, promo)) return SHAYVERI::MOVE_NONE;
    MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i) {
        Move m = legal.moves[i];
        if (SHAYVERI::move_from(m) == from && SHAYVERI::move_to(m) == to && SHAYVERI::move_promo(m) == promo)
            return m;
    }
    return SHAYVERI::MOVE_NONE;
}

int main() {
    SHAYVERI::Zobrist::init();
    init_attacks();

    const std::vector<SeeCase> cases = {
        {"Pawn takes queen (free)", "4k3/8/3q4/4P3/8/8/8/4K3 w - - 0 1", "e5d6", 900},
        {"Queen takes pawn, gets recaptured", "4k3/8/3p4/4Q3/8/8/3r4/4K3 w - - 0 1", "e5d6", -800},
        {"Pawn trade sequence", "4k3/2p5/3p4/4P3/8/8/8/4K3 w - - 0 1", "e5d6", 0},
        {"Rook takes rook (free)", "4k3/8/8/3rR3/8/8/8/4K3 w - - 0 1", "e5d5", 500},
        {"Rook exchange with queen recapture", "3qk3/8/8/3rR3/8/8/8/4K3 w - - 0 1", "e5d5", 0},
        {"En-passant capture", "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6", 100},
        {"Promotion capture", "r3k3/1P6/8/8/8/8/8/4K3 w - - 0 1", "b7a8q", 500},
    };
    const std::vector<int> thresholds = {
        -1000, -500, -161, -1, 0, 1, 100, 330, 500, 900,
    };

    int failures = 0;
    for (const auto &tc : cases) {
        Board b;
        if (!set_from_fen(b, tc.fen)) {
            std::cerr << "[FAIL] " << tc.name << ": bad FEN\n";
            ++failures;
            continue;
        }
        Move m = find_legal_move(b, tc.uci_move);
        if (m == SHAYVERI::MOVE_NONE) {
            std::cerr << "[FAIL] " << tc.name << ": move not legal in position\n";
            ++failures;
            continue;
        }
        int actual = see(b, m);
        if (actual != tc.expected) {
            std::cerr << "[FAIL] " << tc.name << ": expected " << tc.expected << ", got " << actual << "\n";
            ++failures;
            continue;
        }
        bool threshold_ok = true;
        for (int threshold : thresholds) {
            if (see_ge(b, m, threshold) != (actual >= threshold)) {
                std::cerr << "[FAIL] " << tc.name
                          << ": threshold mismatch at " << threshold << "\n";
                threshold_ok = false;
                ++failures;
                break;
            }
        }
        if (threshold_ok) std::cout << "[PASS] " << tc.name << "\n";
    }

    Board random_board;
    if (!set_from_fen(
            random_board,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")) {
        std::cerr << "[FAIL] randomized SEE setup\n";
        ++failures;
    } else {
        std::mt19937 rng(0x5EE);
        int comparisons = 0;
        for (int position = 0; position < 500; ++position) {
            MoveList legal = generate_legal_moves(random_board);
            if (legal.count == 0) {
                set_from_fen(
                    random_board,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                continue;
            }

            for (int i = 0; i < legal.count; ++i) {
                const Move m = legal.moves[i];
                if (random_board.get_piece(SHAYVERI::move_to(m))
                        == SHAYVERI::NONE_PIECE
                    && !SHAYVERI::is_ep_move(m)
                    && SHAYVERI::move_promo(m) == NONE_PTYPE)
                    continue;

                const int actual = see(random_board, m);
                const int probes[] = {
                    actual - 1, actual, actual + 1, -300, 0, 300,
                };
                for (const int threshold : probes) {
                    ++comparisons;
                    if (see_ge(random_board, m, threshold)
                        != (actual >= threshold)) {
                        std::cerr << "[FAIL] randomized SEE threshold mismatch"
                                  << " at position " << position
                                  << ", threshold " << threshold
                                  << ", SEE " << actual
                                  << ", move " << SHAYVERI::move_to_uci(m)
                                  << ", FEN " << SHAYVERI::get_board_fen(random_board)
                                  << "\n";
                        ++failures;
                        position = 500;
                        break;
                    }
                }
                if (position >= 500) break;
            }
            if (position >= 500) break;

            const Move selected =
                legal.moves[static_cast<int>(rng() % legal.count)];
            SHAYVERI::Undo undo;
            if (!SHAYVERI::make_generated_move(
                    random_board, selected, undo)) {
                std::cerr << "[FAIL] randomized SEE playout move\n";
                ++failures;
                break;
            }
            if (random_board.half_move >= 100)
                set_from_fen(
                    random_board,
                    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        }
        if (failures == 0)
            std::cout << "[PASS] randomized SEE thresholds ("
                      << comparisons << " comparisons)\n";
    }

    if (failures == 0) {
        std::cout << "SEE suite passed: " << cases.size() << "/" << cases.size() << "\n";
        return 0;
    }
    std::cerr << "SEE suite failed: " << failures << "/" << cases.size() << " failed\n";
    return 2;
}
