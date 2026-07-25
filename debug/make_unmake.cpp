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
using SHAYVERI::MOVE_NONE;
using SHAYVERI::File;
using SHAYVERI::Rank;
using SHAYVERI::create_ep_move;
using SHAYVERI::create_move;
using SHAYVERI::find_first_legal_move;
using SHAYVERI::generate_legal_moves;
using SHAYVERI::generate_pseudo_legal_moves;
using SHAYVERI::init_attacks;
using SHAYVERI::is_legal_move;
using SHAYVERI::make_move;
using SHAYVERI::make_generated_move;
using SHAYVERI::set_from_fen;
using SHAYVERI::unmake_move;

static bool boards_equal(const Board &a, const Board &b) {
    return a.bit_boards == b.bit_boards
        && a.occupancies == b.occupancies
        && a.mailbox == b.mailbox
        && a.hash == b.hash
        && a.pawn_hash == b.pawn_hash
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
        if (!make_generated_move(b, m, u)) continue;
        if (!b.is_consistent()
            || b.hash != SHAYVERI::Zobrist::compute(b)
            || b.pawn_hash != SHAYVERI::Zobrist::compute_pawns(b))
            return false;
        ++checked;

        if (!verify_roundtrip(b, depth - 1, checked)) return false;

        unmake_move(b, m, u);
        if (!boards_equal(b, before)) return false;
    }
    return true;
}

static bool verify_legality_helpers(Board &b) {
    const Board before = b;
    const MoveList legal = generate_legal_moves(b);

    const Move expected_first = legal.count > 0 ? legal.moves[0] : MOVE_NONE;
    if (find_first_legal_move(b) != expected_first || !boards_equal(b, before)) return false;
    if (is_legal_move(b, MOVE_NONE) || !boards_equal(b, before)) return false;

    for (int i = 0; i < legal.count; ++i) {
        if (!is_legal_move(b, legal.moves[i]) || !boards_equal(b, before)) return false;
    }

    const MoveList pseudo = generate_pseudo_legal_moves(b);
    for (int i = 0; i < pseudo.count; ++i) {
        Board checked = b;
        Board trusted = b;
        Undo checked_undo;
        Undo trusted_undo;
        const bool checked_legal = make_move(checked, pseudo.moves[i], checked_undo);
        const bool trusted_legal =
            make_generated_move(trusted, pseudo.moves[i], trusted_undo);
        if (checked_legal != trusted_legal || !boards_equal(checked, trusted))
            return false;
        if (checked_legal) {
            unmake_move(checked, pseudo.moves[i], checked_undo);
            unmake_move(trusted, pseudo.moves[i], trusted_undo);
            if (!boards_equal(checked, b) || !boards_equal(trusted, b))
                return false;
        }

        bool legal_match = false;
        for (int j = 0; j < legal.count; ++j) {
            if (pseudo.moves[i] == legal.moves[j]) {
                legal_match = true;
                break;
            }
        }
        if (!legal_match) {
            if (is_legal_move(b, pseudo.moves[i]) || !boards_equal(b, before)) return false;
            break;
        }
    }
    return true;
}

static bool verify_malformed_moves() {
    Board b;
    if (!SHAYVERI::set_startpos(b)) return false;
    const Board before = b;

    const Move malformed[] = {
        create_move(SHAYVERI::make_square(File::FILE_A, Rank::RANK_1),
                    SHAYVERI::make_square(File::FILE_A, Rank::RANK_4)),
        create_move(SHAYVERI::make_square(File::FILE_E, Rank::RANK_2),
                    SHAYVERI::make_square(File::FILE_E, Rank::RANK_5)),
        create_move(SHAYVERI::make_square(File::FILE_B, Rank::RANK_1),
                    SHAYVERI::make_square(File::FILE_D, Rank::RANK_2)),
        create_move(SHAYVERI::make_square(File::FILE_E, Rank::RANK_2),
                    SHAYVERI::make_square(File::FILE_E, Rank::RANK_3),
                    SHAYVERI::QUEEN),
        create_ep_move(SHAYVERI::make_square(File::FILE_E, Rank::RANK_2),
                       SHAYVERI::make_square(File::FILE_D, Rank::RANK_3)),
    };
    for (Move move : malformed) {
        Undo undo;
        if (make_move(b, move, undo) || !boards_equal(b, before))
            return false;
    }
    return true;
}

int main() {
    SHAYVERI::Zobrist::init();
    init_attacks();

    if (!verify_malformed_moves()) {
        std::cerr << "[FAIL] malformed move validation\n";
        return 2;
    }

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
        if (!verify_legality_helpers(b)) {
            std::cerr << "[FAIL] " << name << ": legality helper mismatch\n";
            ++failures;
        } else if (!verify_roundtrip(b, 3, checked)) {
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
