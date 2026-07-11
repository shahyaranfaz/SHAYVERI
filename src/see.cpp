#include "see.h"

#include "attacks.h"
#include "make.h"
#include "move.h"
#include "tune.h"

#include <algorithm>

namespace SHAYVERI {

using namespace Tune;

static inline U64 attackers_to(const Board &b, Square sq, U64 occ) {
    int f = get_file(sq), r = get_rank(sq);

    U64 wp_mask = 0, bp_mask = 0;
    if (r > 0) {
        if (f > 0) wp_mask |= bb_square(make_square(File(f-1), Rank(r-1)));
        if (f < 7) wp_mask |= bb_square(make_square(File(f+1), Rank(r-1)));
    }
    if (r < 7) {
        if (f > 0) bp_mask |= bb_square(make_square(File(f-1), Rank(r+1)));
        if (f < 7) bp_mask |= bb_square(make_square(File(f+1), Rank(r+1)));
    }

    U64 n_atk = knight_attacks(sq);
    U64 k_atk = king_attacks(sq);
    U64 b_atk = bishop_attacks(sq, occ);
    U64 r_atk = rook_attacks(sq, occ);

    U64 attacks = (wp_mask & b.bit_boards[WP]) | (bp_mask & b.bit_boards[BP]);
    attacks |= n_atk & (b.bit_boards[WN] | b.bit_boards[BN]);
    attacks |= k_atk & (b.bit_boards[WK] | b.bit_boards[BK]);
    attacks |= b_atk & (b.bit_boards[WB] | b.bit_boards[BB] | b.bit_boards[WQ] | b.bit_boards[BQ]);
    attacks |= r_atk & (b.bit_boards[WR] | b.bit_boards[BR] | b.bit_boards[WQ] | b.bit_boards[BQ]);
    return attacks & occ;
}

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
        if (std::max(-gain[depth - 1], gain[depth]) < 0) break;

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

int quiet_see(const Board &b, Move m) {
    if (move_promo(m) != NONE_PTYPE || is_ep_move(m)
        || b.get_piece(move_to(m)) != NONE_PIECE)
        return 0;

    Board post = b;
    Undo u;
    if (!make_move(post, m, u)) return 0;

    // Checking moves remain visible to the full search and are never treated
    // as losing quiet exchanges by this approximation.
    const Square king = king_square(post, post.side_to_move);
    if (is_square_attacked(post, king, flip(post.side_to_move)))
        return MATE_SCORE;

    const Square target = move_to(m);
    U64 attackers = attackers_to(post, target, post.occupied)
        & post.occupancies[post.side_to_move];
    int best_gain = 0;
    while (attackers) {
        Piece attacker_piece;
        U64 attacker = pick_least_valuable_attacker(post, post.side_to_move,
                                                     attackers, attacker_piece);
        if (!attacker) break;
        attackers &= ~attacker;

        const Square from = static_cast<Square>(__builtin_ctzll(attacker));
        Move reply = create_move(from, target);

        Board reply_board = post;
        Undo reply_undo;
        if (!make_move(reply_board, reply, reply_undo)) continue;
        best_gain = std::max(best_gain, see(post, reply));
    }

    return -best_gain;
}

} // namespace SHAYVERI
