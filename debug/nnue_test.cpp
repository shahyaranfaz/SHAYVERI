#include "attacks.h"
#include "board.h"
#include "make.h"
#include "move_gen.h"
#include "nnue.h"
#include "nnue_update.h"
#include "zobrist.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
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

static int scalar_nnue_evaluate(int side_to_move, int piece_count,
                                const NNUE::Accumulator &acc) {
    I64 sum = 0;
    const int hidden_size = NNUE::active_hidden_size();
    const int output_bucket = NNUE::output_bucket_count() == 1
        ? 0 : NNUE::material_bucket(piece_count);
    const I16 *weights = NNUE::output_weights[output_bucket];
    for (int perspective = 0; perspective < 2; ++perspective) {
        const int accumulator_side = side_to_move ^ perspective;
        const int weight_offset = perspective * hidden_size;
        for (int i = 0; i < hidden_size; ++i) {
            const I32 value = std::clamp<I32>(
                acc.vals[accumulator_side][i], 0, NNUE::L1_SCALE);
            const I32 activated = NNUE::uses_screlu()
                ? value * value / NNUE::L1_SCALE : value;
            sum += static_cast<I64>(activated)
                * weights[weight_offset + i];
        }
    }
    sum += NNUE::output_bias[output_bucket];
    return static_cast<int>(
        sum * NNUE::OUTPUT_SCALE / (NNUE::L1_SCALE * NNUE::L1_SCALE));
}

static int check_vector_evaluation() {
    NNUE::Accumulator acc;
    static constexpr I16 VALUES[] = {
        std::numeric_limits<I16>::min(), -1, 0, 1, 127, 254, 255, 256,
        std::numeric_limits<I16>::max(),
    };

    I16 saved_weights[NNUE::MAX_OUTPUT_BUCKETS][NNUE::MAX_HIDDEN_SIZE * 2];
    std::memcpy(saved_weights, NNUE::output_weights, sizeof(saved_weights));
    I32 saved_bias[NNUE::MAX_OUTPUT_BUCKETS];
    std::memcpy(saved_bias, NNUE::output_bias, sizeof(saved_bias));
    const int hidden_size = NNUE::active_hidden_size();

    for (int i = 0; i < hidden_size; ++i) {
        acc.vals[WHITE][i] = VALUES[i % std::size(VALUES)];
        acc.vals[BLACK][i] = VALUES[(i * 5 + 3) % std::size(VALUES)];
        for (int bucket = 0; bucket < NNUE::output_bucket_count(); ++bucket) {
            NNUE::output_weights[bucket][i] = ((i + bucket) & 1)
                ? std::numeric_limits<I16>::min()
                : std::numeric_limits<I16>::max();
            NNUE::output_weights[bucket][hidden_size + i] = ((i + bucket) & 2)
                ? std::numeric_limits<I16>::max()
                : std::numeric_limits<I16>::min();
        }
    }
    for (int bucket = 0; bucket < NNUE::output_bucket_count(); ++bucket)
        NNUE::output_bias[bucket] =
            std::numeric_limits<I32>::max() - bucket * 1000;

    int status = 0;
    const int max_piece_count =
        NNUE::output_bucket_count() == 1 ? 1 : 32;
    for (int piece_count = 1; piece_count <= max_piece_count; ++piece_count) {
        for (int side = WHITE; side <= BLACK; ++side) {
            const int expected = scalar_nnue_evaluate(side, piece_count, acc);
            const int actual = NNUE::evaluate(side, piece_count, acc);
            if (actual != expected) {
                std::cerr << "NNUE vector evaluation mismatch: side=" << side
                          << " piece_count=" << piece_count
                          << " expected=" << expected
                          << " actual=" << actual << "\n";
                status = 1;
            }
        }
    }

    std::memcpy(NNUE::output_weights, saved_weights, sizeof(saved_weights));
    std::memcpy(NNUE::output_bias, saved_bias, sizeof(saved_bias));
    return status;
}

static int check_material_bucket_mapping() {
    for (int piece_count = 1; piece_count <= 32; ++piece_count) {
        const int expected = std::min((piece_count - 1) / 4, 7);
        const int actual = NNUE::material_bucket(piece_count);
        if (actual != expected) {
            std::cerr << "NNUE material bucket mismatch: piece_count="
                      << piece_count << " expected=" << expected
                      << " actual=" << actual << "\n";
            return 1;
        }
    }
    return 0;
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

static int check_position(Board &b, int &quiet_moves_checked,
                          int &quiet_king_moves_checked,
                          int &ordinary_captures_checked) {
    NNUE::Accumulator parent;
    parent.refresh(b);

    MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i) {
        Move m = legal.moves[i];
        Board copy = b;
        const Piece moved = b.get_piece(move_from(m));

        NNUE::Accumulator child;
        Undo u;
        if (!make_generated_move(copy, m, u))
            continue;
        NNUE::update_accumulator(child, parent, copy, m, u);

        NNUE::Accumulator refreshed;
        refreshed.refresh(copy);
        if (!same_acc(child, refreshed)) {
            std::cerr << "NNUE accumulator mismatch after "
                      << test_move_to_uci(m) << "\n";
            return 1;
        }

        if (u.captured == NONE_PIECE && move_promo(m) == NONE_PTYPE &&
            get_type(moved) != KING && !is_ep_move(m))
            ++quiet_moves_checked;
        if (NNUE::has_king_buckets() && u.captured == NONE_PIECE &&
            get_type(moved) == KING &&
            std::abs(get_file(move_from(m)) - get_file(move_to(m))) != 2)
            ++quiet_king_moves_checked;
        if (u.captured != NONE_PIECE && move_promo(m) == NONE_PTYPE &&
            get_type(moved) != KING && !is_ep_move(m))
            ++ordinary_captures_checked;
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
    if (check_material_bucket_mapping() != 0)
        return 1;

    Zobrist::init();
    init_attacks();
    std::string error;
    if (!NNUE::load(argv[1], error)) {
        std::cerr << "failed to load NNUE: " << error << "\n";
        return 1;
    }
    if (check_vector_evaluation() != 0)
        return 1;

    Board b;
    int quiet_moves_checked = 0;
    int quiet_king_moves_checked = 0;
    int ordinary_captures_checked = 0;
    set_startpos(b);
    if (check_position(b, quiet_moves_checked, quiet_king_moves_checked,
                       ordinary_captures_checked) != 0)
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
        if (check_position(b, quiet_moves_checked, quiet_king_moves_checked,
                           ordinary_captures_checked) != 0)
            return 1;
    }

    if (quiet_moves_checked == 0) {
        std::cerr << "NNUE quiet-move accumulator path was not tested\n";
        return 1;
    }
    if (NNUE::has_king_buckets() && quiet_king_moves_checked == 0) {
        std::cerr << "NNUE selective king-perspective refresh was not tested\n";
        return 1;
    }
    if (ordinary_captures_checked == 0) {
        std::cerr << "NNUE ordinary-capture accumulator path was not tested\n";
        return 1;
    }

    std::cout << "NNUE accumulator tests passed (including "
              << quiet_moves_checked << " quiet moves and "
              << quiet_king_moves_checked << " quiet king moves and "
              << ordinary_captures_checked << " ordinary captures)\n";
    return 0;
}
