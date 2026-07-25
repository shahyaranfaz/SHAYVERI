#include "move_gen.h"

#include "attacks.h"
#include "make.h"

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
    MoveList pseudo = generate_pseudo_legal_moves(b);
    for (int i = 0; i < pseudo.count; ++i) {
        const Move move = pseudo.moves[i];
        if (!find_first && move != target) continue;

        Undo u;
        if (!make_generated_move(b, move, u)) continue;
        unmake_move(b, move, u);
        return move;
    }
    return MOVE_NONE;
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
    MoveList pseudo = generate_pseudo_legal_moves(b);
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
