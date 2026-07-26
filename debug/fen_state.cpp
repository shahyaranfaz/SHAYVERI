#include "board.h"
#include "types.h"
#include "zobrist.h"

#include <iostream>
#include <string>
#include <vector>

using SHAYVERI::BLACK;
using SHAYVERI::BK;
using SHAYVERI::Board;
using SHAYVERI::BP;
using SHAYVERI::BR;
using SHAYVERI::Colour;
using SHAYVERI::File;
using SHAYVERI::NONE_PIECE;
using SHAYVERI::Piece;
using SHAYVERI::Rank;
using SHAYVERI::SQ_NONE;
using SHAYVERI::Square;
using SHAYVERI::U64;
using SHAYVERI::WHITE;
using SHAYVERI::WHITE_KINGSIDE;
using SHAYVERI::WHITE_QUEENSIDE;
using SHAYVERI::BLACK_KINGSIDE;
using SHAYVERI::BLACK_QUEENSIDE;
using SHAYVERI::WR;
using SHAYVERI::WP;
using SHAYVERI::WK;
using SHAYVERI::make_square;
using SHAYVERI::set_from_fen;
using SHAYVERI::set_startpos;

struct PieceExpectation {
    Square square;
    Piece piece;
};

struct FenStateCase {
    const char *name;
    const char *fen;
    Colour side;
    int castling;
    Square en_passant;
    int halfmove;
    int fullmove;
    std::vector<PieceExpectation> pieces;
};

int main() {
    // Source examples adapted from python-chess test coverage for FEN state fields.
    const std::vector<FenStateCase> cases = {
        {
            "Start position state",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            WHITE,
            WHITE_KINGSIDE | WHITE_QUEENSIDE | BLACK_KINGSIDE | BLACK_QUEENSIDE,
            SQ_NONE,
            0,
            1,
            {
                {make_square(File::FILE_E, Rank::RANK_1), WK},
                {make_square(File::FILE_E, Rank::RANK_8), BK},
                {make_square(File::FILE_A, Rank::RANK_2), WP},
                {make_square(File::FILE_H, Rank::RANK_7), BP},
            }
        },
        {
            "EP and counters state",
            "rnbqkbnr/pppppp2/7p/6pP/8/8/PPPPPPP1/RNBQKBNR w KQkq g6 7 14",
            WHITE,
            WHITE_KINGSIDE | WHITE_QUEENSIDE | BLACK_KINGSIDE | BLACK_QUEENSIDE,
            make_square(File::FILE_G, Rank::RANK_6),
            7,
            14,
            {
                {make_square(File::FILE_H, Rank::RANK_5), WP},
                {make_square(File::FILE_G, Rank::RANK_5), BP},
            }
        },
        {
            "Partial castling rights",
            "r3k2r/8/8/8/8/8/8/R3K2R b Kq - 10 42",
            BLACK,
            WHITE_KINGSIDE | BLACK_QUEENSIDE,
            SQ_NONE,
            10,
            42,
            {
                {make_square(File::FILE_A, Rank::RANK_1), WR},
                {make_square(File::FILE_H, Rank::RANK_1), WR},
            }
        },
    };

    SHAYVERI::Zobrist::init();

    int failures = 0;
    for (const auto &tc : cases) {
        Board b;
        if (!set_from_fen(b, tc.fen)) {
            std::cerr << "[FAIL] " << tc.name << ": failed to parse FEN\n";
            ++failures;
            continue;
        }

        bool ok = true;
        if (b.side_to_move != tc.side) ok = false;
        if (b.castling != tc.castling) ok = false;
        if (b.en_passant != tc.en_passant) ok = false;
        if (b.half_move != tc.halfmove) ok = false;
        if (b.full_move != tc.fullmove) ok = false;
        if (!b.is_consistent() || !b.is_plausible_position()) ok = false;
        if (b.hash != SHAYVERI::Zobrist::compute(b)
            || b.pawn_hash != SHAYVERI::Zobrist::compute_pawns(b))
            ok = false;

        for (const auto &p : tc.pieces) {
            if (b.get_piece(p.square) != p.piece) ok = false;
        }

        if (!ok) {
            std::cerr << "[FAIL] " << tc.name << ": state mismatch\n";
            ++failures;
        } else {
            std::cout << "[PASS] " << tc.name << "\n";
        }
    }

    const std::vector<std::string> invalid = {
        "8/8/8/8/8/8/8 w - - 0 1",
        "9/8/8/8/8/8/8/8 w - - 0 1",
        "7/8/8/8/8/8/8/8 w - - 0 1",
        "8/8/8/8/8/8/8/8 w - - -1 1",
        "8/8/8/8/8/8/8/8 w - - 0 0",
        "8/8/8/8/8/8/8/8 w - - 0 1 trailing",
        "8/8/8/8/8/8/8/K7 w - - 0 1",
        "k7/8/8/8/8/8/8/8 w - - 0 1",
        "k7/8/8/8/8/8/8/KK6 w - - 0 1",
        "kk6/8/8/8/8/8/8/K7 w - - 0 1",
        "kppppppp/pppppppp/pppppppp/8/8/PPPPPPPP/PPPPPPPP/KPPPPPPP w - - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w KK - 0 1",
        "4k3/8/8/8/8/8/8/4K3 w K - 0 1",
        "4k3/8/8/8/8/8/8/R3K3 w K - 0 1",
        "4k3/8/8/8/8/8/8/P3K3 w - - 0 1",
        "4k3/pppppppp/p7/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/PPPPPPPP/P3K3 w - - 0 1",
        "8/8/8/8/8/8/4k3/4K3 w - - 0 1",
        "4k3/6p1/8/6p1/8/8/8/4K3 w - g6 0 1",
    };
    for (const std::string &fen : invalid) {
        Board b;
        set_startpos(b);
        const std::string before = SHAYVERI::get_board_fen(b);
        const U64 hash_before = b.hash;
        if (set_from_fen(b, fen) || SHAYVERI::get_board_fen(b) != before || b.hash != hash_before) {
            std::cerr << "[FAIL] malformed FEN accepted or changed board: " << fen << "\n";
            ++failures;
        }
    }

    Board corrupted;
    set_startpos(corrupted);
    corrupted.mailbox[make_square(File::FILE_A, Rank::RANK_2)] = NONE_PIECE;
    if (corrupted.is_consistent()) {
        std::cerr << "[FAIL] mailbox corruption was not detected\n";
        ++failures;
    }
    set_startpos(corrupted);
    corrupted.pawn_hash ^= 1;
    if (corrupted.is_consistent()) {
        std::cerr << "[FAIL] pawn-hash corruption was not detected\n";
        ++failures;
    }

    if (failures == 0) {
        std::cout << "FEN state suite passed: " << cases.size()
                  << " valid + " << invalid.size() << " malformed cases\n";
        return 0;
    }

    std::cerr << "FEN state suite failed: " << failures << "/" << cases.size() << " failed\n";
    return 2;
}
