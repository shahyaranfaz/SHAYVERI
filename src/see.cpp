#include "see.h"

#include "attacks.h"
#include "make.h"
#include "move.h"
#include "tune.h"

#include <algorithm>

namespace SHAYVERI {

using namespace Tune;

static inline int ptype_value(Piece p) {
    return PTYPE_VALUE[get_type(p)];
}

static inline U64 pick_least_valuable_attacker(const Board &b, Colour side, U64 atks, Piece &piece_out) {
    auto pick = [&](U64 bb, Piece p) -> U64 {
        if (!bb) return 0;
        piece_out = p;
        return bb_square(__builtin_ctzll(bb));
    };

    if (side == WHITE) {
        if (U64 bb = atks & b.bit_boards[WP]) return pick(bb, WP);
        if (U64 bb = atks & b.bit_boards[WN]) return pick(bb, WN);
        if (U64 bb = atks & b.bit_boards[WB]) return pick(bb, WB);
        if (U64 bb = atks & b.bit_boards[WR]) return pick(bb, WR);
        if (U64 bb = atks & b.bit_boards[WQ]) return pick(bb, WQ);
        if (U64 bb = atks & b.bit_boards[WK]) return pick(bb, WK);
    } else {
        if (U64 bb = atks & b.bit_boards[BP]) return pick(bb, BP);
        if (U64 bb = atks & b.bit_boards[BN]) return pick(bb, BN);
        if (U64 bb = atks & b.bit_boards[BB]) return pick(bb, BB);
        if (U64 bb = atks & b.bit_boards[BR]) return pick(bb, BR);
        if (U64 bb = atks & b.bit_boards[BQ]) return pick(bb, BQ);
        if (U64 bb = atks & b.bit_boards[BK]) return pick(bb, BK);
    }
    piece_out = NONE_PIECE;
    return 0;
}

int see(const Board &b, Move m) {
    Square from = move_from(m);
    Square to   = move_to(m);

    Piece attacker_orig = b.get_piece(from);
    if (attacker_orig == NONE_PIECE) return 0;

    Piece  captured_orig = b.get_piece(to);
    Square ep_cap_sq     = SQ_NONE;
    if (captured_orig == NONE_PIECE) {
        if (is_ep_move(m)) {
            ep_cap_sq    = (get_colour(attacker_orig) == WHITE) ? to - 8 : to + 8;
            captured_orig = b.get_piece(ep_cap_sq);
        }
        if (captured_orig == NONE_PIECE) return 0;
    }

    int gain[32];
    int depth = 0;
    U64 occ = b.occupied;
    occ &= ~bb_square(from);
    if (ep_cap_sq != SQ_NONE) occ &= ~bb_square(ep_cap_sq);
    gain[depth] = ptype_value(captured_orig);

    Colour side         = flip(get_colour(attacker_orig));
    occ                |= bb_square(to);
    U64   atks          = attackers_to(b, to, occ) & ~bb_square(to);
    Piece last_attacker = attacker_orig;
    depth++;

    while (true) {
        Piece picked_piece;
        U64 pick = pick_least_valuable_attacker(b, side, atks, picked_piece);
        if (!pick) break;

        gain[depth] = ptype_value(last_attacker) - gain[depth - 1];
        if (std::max(-gain[depth - 1], gain[depth]) < 0) {
            ++depth;
            break;
        }

        occ  &= ~pick;
        atks  = attackers_to(b, to, occ) & ~bb_square(to);
        last_attacker = picked_piece;
        side  = flip(side);
        depth++;
        if (depth >= 31) break;
    }

    while (--depth)
        gain[depth - 1] = -std::max(-gain[depth - 1], gain[depth]);

    return gain[0];
}

bool see_ge(const Board &b, Move m, int threshold) {
    const Square from = move_from(m);
    const Square to = move_to(m);
    const Piece attacker = b.get_piece(from);
    if (attacker == NONE_PIECE) return threshold <= 0;

    Piece captured = b.get_piece(to);
    Square ep_capture = SQ_NONE;
    if (captured == NONE_PIECE && is_ep_move(m)) {
        ep_capture = get_colour(attacker) == WHITE ? to - 8 : to + 8;
        captured = b.get_piece(ep_capture);
    }
    if (captured == NONE_PIECE) return threshold <= 0;

    const int captured_value = ptype_value(captured);
    if (captured_value < threshold) return false;
    if (captured_value - ptype_value(attacker) >= threshold) return true;
    return see(b, m) >= threshold;
}

int quiet_see_after_move(Board &b, Square target) {
    U64 attackers = attackers_to(b, target, b.occupied)
        & b.occupancies[b.side_to_move];
    int best_gain = 0;
    while (attackers) {
        Piece attacker_piece;
        U64 attacker = pick_least_valuable_attacker(b, b.side_to_move,
                                                     attackers, attacker_piece);
        if (!attacker) break;
        attackers &= ~attacker;

        const Square from = static_cast<Square>(__builtin_ctzll(attacker));
        Move reply = create_move(from, target);

        Undo reply_undo;
        if (!make_generated_move(b, reply, reply_undo)) continue;
        unmake_move(b, reply, reply_undo);
        best_gain = std::max(best_gain, see(b, reply));
    }

    return -best_gain;
}

} // namespace SHAYVERI
