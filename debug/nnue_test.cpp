#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move_gen.h"
#include "nnue.h"
#include "nnue_update.h"
#include "zobrist.h"

#include <cstring>
#include <iostream>
#include <string>

using namespace SHAYVERI;

static int marlinflow_board768_feature(int perspective, int piece_colour,
                                       int piece_type, int sq) {
    int effective_sq = perspective == BLACK ? (sq ^ 56) : sq;
    int effective_colour = perspective == BLACK ? (piece_colour ^ 1) : piece_colour;
    return ((effective_colour * 6) + piece_type) * 64 + effective_sq;
}

static int check_feature_index_mapping() {
    for (int perspective = WHITE; perspective <= BLACK; ++perspective) {
        for (int piece_colour = WHITE; piece_colour <= BLACK; ++piece_colour) {
            for (int piece_type = 0; piece_type < 6; ++piece_type) {
                for (int sq = 0; sq < 64; ++sq) {
                    int expected = marlinflow_board768_feature(
                        perspective, piece_colour, piece_type, sq);
                    int actual = NNUE::chess768_index(
                        piece_type, piece_colour, sq, perspective);
                    if (actual != expected) {
                        std::cerr << "NNUE feature index mismatch: perspective="
                                  << perspective << " colour=" << piece_colour
                                  << " type=" << piece_type << " sq=" << sq
                                  << " expected=" << expected
                                  << " actual=" << actual << "\n";
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

static int check_king_bucket_mapping() {
    for (int perspective = WHITE; perspective <= BLACK; ++perspective) {
        for (int sq = 0; sq < 64; ++sq) {
            int effective_sq = perspective == BLACK ? (sq ^ 56) : sq;
            int file = effective_sq & 7;
            int rank = effective_sq >> 3;
            static constexpr int FILE_MAP[8] = {0, 1, 2, 3, 3, 2, 1, 0};
            int expected = (rank >= 4 ? 4 : 0) + FILE_MAP[file];
            int actual = NNUE::king_bucket_index(sq, perspective, 8);
            if (actual != expected) {
                std::cerr << "NNUE king bucket mismatch: perspective=" << perspective
                          << " sq=" << sq
                          << " expected=" << expected
                          << " actual=" << actual << "\n";
                return 1;
            }
        }
    }
    return 0;
}

static bool same_acc(const NNUE::Accumulator &a, const NNUE::Accumulator &b) {
    return std::memcmp(a.vals, b.vals, sizeof(a.vals)) == 0;
}

static std::string test_move_to_uci(Move m) {
    Square from = move_from(m);
    Square to = move_to(m);
    std::string out;
    out += char('a' + get_file(from));
    out += char('1' + get_rank(from));
    out += char('a' + get_file(to));
    out += char('1' + get_rank(to));

    switch (move_promo(m)) {
        case KNIGHT: out += 'n'; break;
        case BISHOP: out += 'b'; break;
        case ROOK:   out += 'r'; break;
        case QUEEN:  out += 'q'; break;
        default: break;
    }

    return out;
}

static int check_position(Board &b) {
    NNUE::Accumulator parent;
    parent.refresh(b);

    MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i) {
        Move m = legal.moves[i];
        Board copy = b;

        NNUE::Accumulator child;
        Undo u;
        if (!make_move(copy, m, u))
            continue;
        NNUE::update_accumulator(child, parent, copy, m, u);

        NNUE::Accumulator refreshed;
        refreshed.refresh(copy);
        if (!same_acc(child, refreshed)) {
            std::cerr << "NNUE accumulator mismatch after "
                      << test_move_to_uci(m) << "\n";
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: nnue_test <net.nnue>\n";
        return 1;
    }

    if (check_feature_index_mapping() != 0)
        return 1;
    if (check_king_bucket_mapping() != 0)
        return 1;

    Zobrist::init();
    init_attacks();
    std::string error;
    if (!NNUE::load(argv[1], error)) {
        std::cerr << "failed to load NNUE: " << error << "\n";
        return 1;
    }

    Board b;
    set_startpos(b);
    if (check_position(b) != 0)
        return 1;

    const char *fens[] = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/P6k/8/8/8/8/7p/K7 w - - 0 1",
        "8/8/8/3pP3/8/8/8/4K2k w - d6 0 1",
    };

    for (const char *fen : fens) {
        if (!set_from_fen(b, fen)) {
            std::cerr << "failed to parse FEN: " << fen << "\n";
            return 1;
        }
        if (check_position(b) != 0)
            return 1;
    }

    std::cout << "NNUE accumulator tests passed\n";
    return 0;
}
