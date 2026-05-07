#include "attacks.h"
#include "board.h"
#include "move.h"
#include "move_gen.h"
#include "see.h"
#include "zobrist.h"

#include <iostream>
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
        } else {
            std::cout << "[PASS] " << tc.name << "\n";
        }
    }

    if (failures == 0) {
        std::cout << "SEE suite passed: " << cases.size() << "/" << cases.size() << "\n";
        return 0;
    }
    std::cerr << "SEE suite failed: " << failures << "/" << cases.size() << " failed\n";
    return 2;
}
