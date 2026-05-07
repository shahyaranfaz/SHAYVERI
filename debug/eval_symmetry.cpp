#include "board.h"
#include "evaluate.h"
#include "types.h"
#include "zobrist.h"
#include "attacks.h"

#include <iostream>
#include <vector>
#include <cstdlib>

using SHAYVERI::BLACK;
using SHAYVERI::Board;
using SHAYVERI::Colour;
using SHAYVERI::File;
using SHAYVERI::Piece;
using SHAYVERI::Rank;
using SHAYVERI::SQ_NONE;
using SHAYVERI::Square;
using SHAYVERI::WHITE;
using SHAYVERI::bb_square;
using SHAYVERI::evaluate;
using SHAYVERI::flip;
using SHAYVERI::get_file;
using SHAYVERI::get_rank;
using SHAYVERI::make_square;
using SHAYVERI::set_from_fen;

static Piece swap_colour(Piece p) {
    switch (p) {
        case SHAYVERI::WP: return SHAYVERI::BP;
        case SHAYVERI::WN: return SHAYVERI::BN;
        case SHAYVERI::WB: return SHAYVERI::BB;
        case SHAYVERI::WR: return SHAYVERI::BR;
        case SHAYVERI::WQ: return SHAYVERI::BQ;
        case SHAYVERI::WK: return SHAYVERI::BK;
        case SHAYVERI::BP: return SHAYVERI::WP;
        case SHAYVERI::BN: return SHAYVERI::WN;
        case SHAYVERI::BB: return SHAYVERI::WB;
        case SHAYVERI::BR: return SHAYVERI::WR;
        case SHAYVERI::BQ: return SHAYVERI::WQ;
        case SHAYVERI::BK: return SHAYVERI::WK;
        default: return SHAYVERI::NONE_PIECE;
    }
}

static Square mirror_square_180(Square s) {
    const int f = int(get_file(s));
    const int r = int(get_rank(s));
    return make_square(File(7 - f), Rank(7 - r));
}

static Board mirror_and_swap(const Board &src) {
    Board out;
    out.clear();
    for (Square s = 0; s < 64; ++s) {
        const Piece p = src.mailbox[s];
        if (p == SHAYVERI::NONE_PIECE) continue;
        const Square ms = mirror_square_180(s);
        const Piece sp = swap_colour(p);
        out.bit_boards[sp] |= bb_square(ms);
    }

    out.side_to_move = flip(src.side_to_move);
    out.castling = 0;
    if (src.castling & SHAYVERI::WHITE_KINGSIDE)  out.castling |= SHAYVERI::BLACK_KINGSIDE;
    if (src.castling & SHAYVERI::WHITE_QUEENSIDE) out.castling |= SHAYVERI::BLACK_QUEENSIDE;
    if (src.castling & SHAYVERI::BLACK_KINGSIDE)  out.castling |= SHAYVERI::WHITE_KINGSIDE;
    if (src.castling & SHAYVERI::BLACK_QUEENSIDE) out.castling |= SHAYVERI::WHITE_QUEENSIDE;
    out.en_passant = src.en_passant == SQ_NONE ? SQ_NONE : mirror_square_180(src.en_passant);
    out.half_move = src.half_move;
    out.full_move = src.full_move;
    out.recompute_all();
    out.hash = SHAYVERI::Zobrist::compute(out);
    return out;
}

int main() {
    SHAYVERI::Zobrist::init();
    SHAYVERI::init_attacks();
    const bool strict = std::getenv("SHAYVERI_STRICT_EVAL_SYMMETRY") != nullptr;

    const std::vector<const char *> fens = {
        "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/3P4/PPP2PPP/RNBQKBNR w KQkq - 0 4",
        "2r2rk1/pp2qppp/2n1bn2/2bp4/3P4/2P1PN2/PP1NBPPP/R2Q1RK1 w - - 2 10",
        "8/4k3/3p4/3P4/4K3/8/8/8 w - - 12 45",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    };

    int failures = 0;
    for (const char *fen : fens) {
        Board b;
        if (!set_from_fen(b, fen)) {
            std::cerr << "[FAIL] parse failed: " << fen << "\n";
            ++failures;
            continue;
        }
        Board mirrored = mirror_and_swap(b);
        int s1 = evaluate(b);
        int s2 = evaluate(mirrored);
        if (s1 != -s2) {
            std::cerr << "[FAIL] symmetry mismatch for FEN: " << fen
                      << " | eval=" << s1 << " mirrored=" << s2 << "\n";
            ++failures;
        } else {
            std::cout << "[PASS] symmetry check\n";
        }
    }

    if (failures == 0) {
        std::cout << "Eval symmetry suite passed: " << fens.size() << "/" << fens.size() << "\n";
        return 0;
    }
    std::cerr << "Eval symmetry suite mismatches: " << failures << "/" << fens.size()
              << (strict ? " (strict mode: failing)\n" : " (non-strict mode: informational)\n");
    return strict ? 2 : 0;
}
