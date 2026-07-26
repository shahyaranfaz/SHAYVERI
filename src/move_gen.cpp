#include "move_gen.h"

#include "attacks.h"
#include "make.h"

#include <cstdlib>

namespace SHAYVERI {

static void push_moves_from_mask(MoveList &out, Square from, U64 mask) {
    while (mask) {
        Square to = __builtin_ctzll(mask);
        mask &= mask - 1;
        out.add(create_move(from, to));
    }
}

static void add_promotions(MoveList &moves, Square from, Square to) {
    moves.add(create_move(from, to, QUEEN));
    moves.add(create_move(from, to, ROOK));
    moves.add(create_move(from, to, BISHOP));
    moves.add(create_move(from, to, KNIGHT));
}

template<Colour Side, bool CapturesOnly>
static void generate_pawn_moves(const Board &b, MoveList &moves, U64 enemy) {
    constexpr Piece pawn = Side == WHITE ? WP : BP;
    constexpr int push = Side == WHITE ? 8 : -8;
    constexpr Rank start_rank = Side == WHITE ? RANK_2 : RANK_7;
    constexpr Rank promotion_rank = Side == WHITE ? RANK_7 : RANK_2;

    U64 pawns = b.bit_boards[pawn];
    while (pawns) {
        const Square from = pop_lsb(pawns);
        const Rank rank = get_rank(from);
        const Square forward = from + push;

        if constexpr (!CapturesOnly) {
            if (is_valid(forward) && !(b.occupied & bb_square(forward))) {
                if (rank == promotion_rank) {
                    add_promotions(moves, from, forward);
                } else {
                    moves.add(create_move(from, forward));
                    const Square double_push = forward + push;
                    if (rank == start_rank && !(b.occupied & bb_square(double_push)))
                        moves.add(create_move(from, double_push));
                }
            }
        } else if (rank == promotion_rank && !(b.occupied & bb_square(forward))) {
            add_promotions(moves, from, forward);
        }

        U64 captures = pawn_attacks(Side, from) & enemy;
        while (captures) {
            const Square to = pop_lsb(captures);
            if (rank == promotion_rank) add_promotions(moves, from, to);
            else                        moves.add(create_move(from, to));
        }

        if (b.en_passant != SQ_NONE &&
            (pawn_attacks(Side, from) & bb_square(b.en_passant)))
            moves.add(create_ep_move(from, b.en_passant));
    }
}

template<Colour Side>
static void generate_piece_moves(const Board &b, MoveList &moves, U64 targets) {
    constexpr Piece knight = Side == WHITE ? WN : BN;
    constexpr Piece bishop = Side == WHITE ? WB : BB;
    constexpr Piece rook = Side == WHITE ? WR : BR;
    constexpr Piece queen = Side == WHITE ? WQ : BQ;

    U64 pieces = b.bit_boards[knight];
    while (pieces) {
        const Square from = pop_lsb(pieces);
        push_moves_from_mask(moves, from, knight_attacks(from) & targets);
    }
    pieces = b.bit_boards[bishop];
    while (pieces) {
        const Square from = pop_lsb(pieces);
        push_moves_from_mask(moves, from, bishop_attacks(from, b.occupied) & targets);
    }
    pieces = b.bit_boards[rook];
    while (pieces) {
        const Square from = pop_lsb(pieces);
        push_moves_from_mask(moves, from, rook_attacks(from, b.occupied) & targets);
    }
    pieces = b.bit_boards[queen];
    while (pieces) {
        const Square from = pop_lsb(pieces);
        push_moves_from_mask(moves, from, queen_attacks(from, b.occupied) & targets);
    }

    const Square king = king_square(b, Side);
    push_moves_from_mask(moves, king, king_attacks(king) & targets);
}

template<Colour Side>
static void generate_castling_moves(const Board &b, MoveList &moves) {
    constexpr Rank rank = Side == WHITE ? RANK_1 : RANK_8;
    constexpr Piece rook = Side == WHITE ? WR : BR;
    constexpr int kingside_right = Side == WHITE ? WHITE_KINGSIDE : BLACK_KINGSIDE;
    constexpr int queenside_right = Side == WHITE ? WHITE_QUEENSIDE : BLACK_QUEENSIDE;
    constexpr Square king_from = make_square(FILE_E, rank);
    constexpr Square kingside_to = make_square(FILE_G, rank);
    constexpr Square queenside_to = make_square(FILE_C, rank);
    constexpr Square kingside_rook = make_square(FILE_H, rank);
    constexpr Square queenside_rook = make_square(FILE_A, rank);
    constexpr U64 kingside_empty = bb_square(make_square(FILE_F, rank)) |
        bb_square(kingside_to);
    constexpr U64 queenside_empty = bb_square(make_square(FILE_B, rank)) |
        bb_square(queenside_to) | bb_square(make_square(FILE_D, rank));
    constexpr Colour enemy = Side == WHITE ? BLACK : WHITE;

    if ((b.castling & kingside_right) && !(b.occupied & kingside_empty) &&
        b.get_piece(kingside_rook) == rook &&
        !is_square_attacked(b, king_from, enemy) &&
        !is_square_attacked(b, make_square(FILE_F, rank), enemy) &&
        !is_square_attacked(b, kingside_to, enemy))
        moves.add(create_move(king_from, kingside_to));

    if ((b.castling & queenside_right) && !(b.occupied & queenside_empty) &&
        b.get_piece(queenside_rook) == rook &&
        !is_square_attacked(b, king_from, enemy) &&
        !is_square_attacked(b, make_square(FILE_D, rank), enemy) &&
        !is_square_attacked(b, queenside_to, enemy))
        moves.add(create_move(king_from, queenside_to));
}

static Move find_matching_legal_move(Board &b, Move target, bool find_first) {
    const MoveList legal = generate_legal_moves(b);
    for (int i = 0; i < legal.count; ++i) {
        const Move move = legal.moves[i];
        if (!find_first && move != target) continue;
        return move;
    }
    return MOVE_NONE;
}

static bool attacked_with_occupancy(
    const Board &b, Square square, U64 attacker_occupied, U64 occupied) {
    return (attackers_to(b, square, occupied)
            & attacker_occupied) != 0;
}

struct LegalContext {
    Square king = SQ_NONE;
    U64 enemy_occupied = 0;
    U64 checkers = 0;
    U64 evasion_mask = ~0ULL;
    U64 pin_rays[64]{};
    int check_count = 0;
};

static U64 squares_between(Square from, Square to) {
    const int file_delta =
        (get_file(to) > get_file(from)) - (get_file(to) < get_file(from));
    const int rank_delta =
        (get_rank(to) > get_rank(from)) - (get_rank(to) < get_rank(from));
    const int file_distance =
        std::abs(static_cast<int>(get_file(to))
                 - static_cast<int>(get_file(from)));
    const int rank_distance =
        std::abs(static_cast<int>(get_rank(to))
                 - static_cast<int>(get_rank(from)));
    if (file_delta != 0 && rank_delta != 0
        && file_distance != rank_distance)
        return 0;
    if (file_delta == 0 && rank_delta == 0) return 0;

    U64 between = 0;
    int file = static_cast<int>(get_file(from)) + file_delta;
    int rank = static_cast<int>(get_rank(from)) + rank_delta;
    while (file != static_cast<int>(get_file(to))
           || rank != static_cast<int>(get_rank(to))) {
        between |= bb_square(
            make_square(static_cast<File>(file), static_cast<Rank>(rank)));
        file += file_delta;
        rank += rank_delta;
    }
    return between & ~bb_square(to);
}

static LegalContext make_legal_context(const Board &b) {
    LegalContext context;
    const Colour side = b.side_to_move;
    const Colour enemy = flip(side);
    context.king = king_square(b, side);
    context.enemy_occupied =
        b.occupancies[static_cast<int>(enemy)];
    context.checkers =
        attackers_to(b, context.king, b.occupied)
        & context.enemy_occupied;
    context.check_count = __builtin_popcountll(context.checkers);
    if (context.check_count == 1) {
        const Square checker = __builtin_ctzll(context.checkers);
        context.evasion_mask =
            bb_square(checker) | squares_between(context.king, checker);
    } else if (context.check_count > 1) {
        context.evasion_mask = 0;
    }

    const U64 own = b.occupancies[static_cast<int>(side)];
    const U64 enemy_rook_sliders =
        side == WHITE
            ? (b.bit_boards[BR] | b.bit_boards[BQ])
            : (b.bit_boards[WR] | b.bit_boards[WQ]);
    const U64 enemy_bishop_sliders =
        side == WHITE
            ? (b.bit_boards[BB] | b.bit_boards[BQ])
            : (b.bit_boards[WB] | b.bit_boards[WQ]);
    U64 snipers =
        (rook_attacks(context.king, b.occupied ^ own)
             & enemy_rook_sliders)
        | (bishop_attacks(context.king, b.occupied ^ own)
             & enemy_bishop_sliders);
    while (snipers) {
        const Square sniper = pop_lsb(snipers);
        const U64 between = squares_between(context.king, sniper);
        const U64 blockers = between & own;
        if (__builtin_popcountll(blockers) == 1) {
            const Square pinned = __builtin_ctzll(blockers);
            context.pin_rays[pinned] = between | bb_square(sniper);
        }
    }
    return context;
}

static bool is_directly_legal(
    const Board &b, Move move, const LegalContext &context) {
    const Colour side = b.side_to_move;
    const Square from = move_from(move);
    const Square to = move_to(move);
    const Piece moved = b.get_piece(from);
    const bool king_move = get_type(moved) == KING;
    U64 enemy_occupied = context.enemy_occupied;

    U64 occupied = b.occupied & ~bb_square(from);
    if (is_ep_move(move)) {
        const Square captured_square = side == WHITE ? to - 8 : to + 8;
        occupied &= ~bb_square(captured_square);
        enemy_occupied &= ~bb_square(captured_square);
    } else {
        occupied &= ~bb_square(to);
        enemy_occupied &= ~bb_square(to);
    }
    occupied |= bb_square(to);

    if (king_move) {
        const bool castling =
            std::abs(static_cast<int>(get_file(from))
                     - static_cast<int>(get_file(to))) == 2;
        if (castling) {
            const CastleInfo castle =
                castle_info(side, to > from);
            occupied &= ~bb_square(castle.rook_from);
            occupied |= bb_square(castle.rook_to);
            const Square transit =
                from + (to > from ? 1 : -1);
            const U64 transit_occupied =
                (b.occupied & ~bb_square(from)) | bb_square(transit);
            if (attacked_with_occupancy(
                    b, transit, enemy_occupied, transit_occupied))
                return false;
        }
        return !attacked_with_occupancy(
            b, to, enemy_occupied, occupied);
    }

    if (context.check_count > 1) return false;

    const Square captured_square = is_ep_move(move)
        ? (side == WHITE ? to - 8 : to + 8)
        : to;
    if (context.check_count == 1
        && !(context.evasion_mask & bb_square(to))
        && !(context.checkers & bb_square(captured_square)))
        return false;

    const U64 pin_ray = context.pin_rays[from];
    if (pin_ray && !(pin_ray & bb_square(to))) return false;

    // En passant removes a pawn from a square other than the destination and
    // can expose a horizontal, vertical, or diagonal attack on the king.
    if (is_ep_move(move))
        return !attacked_with_occupancy(
            b, context.king, enemy_occupied, occupied);

    return true;
}

MoveList generate_pseudo_legal_moves(Board &b) {
    MoveList moves;

    Colour curr  = b.side_to_move;
    if (curr != WHITE && curr != BLACK) return moves;

    const int curr_idx = (curr == WHITE) ? 0 : 1;
    const int other_idx = curr_idx ^ 1;
    U64 curr_occupied  = b.occupancies[curr_idx];
    U64 other_occupied = b.occupancies[other_idx];

    if (curr == WHITE) generate_pawn_moves<WHITE, false>(b, moves, other_occupied);
    else               generate_pawn_moves<BLACK, false>(b, moves, other_occupied);

    if (curr == WHITE) generate_piece_moves<WHITE>(b, moves, ~curr_occupied);
    else               generate_piece_moves<BLACK>(b, moves, ~curr_occupied);

    if (curr == WHITE) generate_castling_moves<WHITE>(b, moves);
    else               generate_castling_moves<BLACK>(b, moves);

    return moves;
}

// Generates pseudo-legal captures and promotions only (used by qsearch when not in check).
MoveList generate_pseudo_legal_captures(Board &b) {
    MoveList moves;
    Colour curr  = b.side_to_move;
    if (curr != WHITE && curr != BLACK) return moves;

    const int other_idx = (curr == WHITE) ? 1 : 0;
    U64 other_occupied = b.occupancies[other_idx];

    if (curr == WHITE) generate_pawn_moves<WHITE, true>(b, moves, other_occupied);
    else               generate_pawn_moves<BLACK, true>(b, moves, other_occupied);

    if (curr == WHITE) generate_piece_moves<WHITE>(b, moves, other_occupied);
    else               generate_piece_moves<BLACK>(b, moves, other_occupied);

    return moves;
}

MoveList generate_legal_moves(Board &b) {
    const MoveList pseudo = generate_pseudo_legal_moves(b);
    const LegalContext context = make_legal_context(b);
    MoveList legal;
    for (int i = 0; i < pseudo.count; ++i) {
        const Move move = pseudo.moves[i];
        if (is_directly_legal(b, move, context)) legal.add(move);
    }
    return legal;
}

MoveList generate_legal_captures(Board &b) {
    const MoveList pseudo = generate_pseudo_legal_captures(b);
    const LegalContext context = make_legal_context(b);
    MoveList legal;
    for (int i = 0; i < pseudo.count; ++i) {
        const Move move = pseudo.moves[i];
        if (is_directly_legal(b, move, context)) legal.add(move);
    }
    return legal;
}

MoveList generate_legal_moves_checked(Board &b) {
    const MoveList pseudo = generate_pseudo_legal_moves(b);
    MoveList legal;
    for (int i = 0; i < pseudo.count; ++i) {
        const Move move = pseudo.moves[i];
        Undo u;
        if (!make_generated_move(b, move, u)) continue;
        legal.add(move);
        unmake_move(b, move, u);
    }
    return legal;
}

Move find_first_legal_move(Board &b) {
    return find_matching_legal_move(b, MOVE_NONE, true);
}

bool is_legal_move(Board &b, Move move) {
    return move != MOVE_NONE && find_matching_legal_move(b, move, false) != MOVE_NONE;
}

} // namespace SHAYVERI
