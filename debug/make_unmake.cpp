#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move_gen.h"
#include "zobrist.h"

#include <iostream>
#include <string>
#include <vector>

using SHAYVERI::Board;
using SHAYVERI::Move;
using SHAYVERI::MoveList;
using SHAYVERI::Undo;
using SHAYVERI::generate_legal_moves;
using SHAYVERI::init_attacks;
using SHAYVERI::make_move;
using SHAYVERI::set_from_fen;
using SHAYVERI::unmake_move;

static bool boards_equal(const Board &a, const Board &b) {
    return a.bit_boards == b.bit_boards
        && a.occupancies == b.occupancies
        && a.mailbox == b.mailbox
        && a.hash == b.hash
        && a.occupied == b.occupied
        && a.side_to_move == b.side_to_move
        && a.en_passant == b.en_passant
        && a.castling == b.castling
        && a.half_move == b.half_move
        && a.full_move == b.full_move;
}

static bool verify_roundtrip(Board &b, int depth, int &checked) {
    if (depth == 0) return true;

    MoveList moves = generate_legal_moves(b);
    for (int i = 0; i < moves.count; ++i) {
        const Move m = moves.moves[i];
        const Board before = b;

        Undo u;
        if (!make_move(b, m, u)) continue;
        ++checked;

        if (!verify_roundtrip(b, depth - 1, checked)) return false;

        unmake_move(b, m, u);
        if (!boards_equal(b, before)) return false;
    }
    return true;
}

int main() {
    SHAYVERI::Zobrist::init();
    init_attacks();

    std::vector<std::pair<std::string, std::string>> roots = {
        {"Start position", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"},
        {"EP legality root", "8/8/8/2k5/2pP4/8/4K3/8 b - d3 0 1"},
        {"Promotion race root", "k7/P7/8/8/8/8/7p/7K w - - 0 1"},
    };

    int failures = 0;
    int checked_total = 0;
    for (const auto &[name, fen] : roots) {
        Board b;
        if (!set_from_fen(b, fen)) {
            std::cerr << "[FAIL] " << name << ": bad FEN\n";
            ++failures;
            continue;
        }

        int checked = 0;
        if (!verify_roundtrip(b, 3, checked)) {
            std::cerr << "[FAIL] " << name << ": make/unmake mismatch\n";
            ++failures;
        } else {
            std::cout << "[PASS] " << name << " (" << checked << " roundtrips)\n";
        }
        checked_total += checked;
    }

    if (failures == 0) {
        std::cout << "Make/unmake suite passed with " << checked_total << " verified roundtrips\n";
        return 0;
    }
    std::cerr << "Make/unmake suite failed: " << failures << "/" << roots.size() << " roots failed\n";
    return 2;
}
