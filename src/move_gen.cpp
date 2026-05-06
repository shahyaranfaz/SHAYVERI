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

MoveList generate_pseudo_legal_moves(Board &b) {
    MoveList moves;
    Colour curr  = b.side_to_move;
    Colour other = flip(curr);

    U64 curr_occupied  = b.occupancies[curr];
    U64 other_occupied = b.occupancies[other];
    U64 empty          = ~b.occupied;

    if (curr == WHITE) {
        U64 pawns = b.bit_boards[WP];
        while (pawns) {
            Square from = __builtin_ctzll(pawns);
            pawns &= pawns - 1;
            int r = get_rank(from);

            Square to_one = from + 8;
            if (to_one < 64 && (empty & bb_square(to_one))) {
                if (r == RANK_7) {
                    moves.add(create_move(from, to_one, QUEEN));
                    moves.add(create_move(from, to_one, ROOK));
                    moves.add(create_move(from, to_one, BISHOP));
                    moves.add(create_move(from, to_one, KNIGHT));
                } else {
                    moves.add(create_move(from, to_one));
                    if (r == RANK_2) {
                        Square to_two = from + 16;
                        if (empty & bb_square(to_two)) moves.add(create_move(from, to_two));
                    }
                }
            }

            U64 captures = pawn_attacks(WHITE, from) & other_occupied;
            while (captures) {
                Square to = __builtin_ctzll(captures);
                captures &= captures - 1;
                if (r == RANK_7) {
                    moves.add(create_move(from, to, QUEEN));
                    moves.add(create_move(from, to, ROOK));
                    moves.add(create_move(from, to, BISHOP));
                    moves.add(create_move(from, to, KNIGHT));
                } else {
                    moves.add(create_move(from, to));
                }
            }

            if (b.en_passant != SQ_NONE) {
                U64 ep_mask = pawn_attacks(WHITE, from) & bb_square(b.en_passant);
                if (ep_mask) moves.add(create_ep_move(from, b.en_passant));
            }
        }
    } else {
        U64 pawns = b.bit_boards[BP];
        while (pawns) {
            Square from = __builtin_ctzll(pawns);
            pawns &= pawns - 1;
            int r = get_rank(from);

            Square to_one = from - 8;
            if (to_one >= 0 && (empty & bb_square(to_one))) {
                if (r == RANK_2) {
                    moves.add(create_move(from, to_one, QUEEN));
                    moves.add(create_move(from, to_one, ROOK));
                    moves.add(create_move(from, to_one, BISHOP));
                    moves.add(create_move(from, to_one, KNIGHT));
                } else {
                    moves.add(create_move(from, to_one));
                    if (r == RANK_7) {
                        Square to_two = from - 16;
                        if (empty & bb_square(to_two)) moves.add(create_move(from, to_two));
                    }
                }
            }

            U64 captures = pawn_attacks(BLACK, from) & other_occupied;
            while (captures) {
                Square to = __builtin_ctzll(captures);
                captures &= captures - 1;
                if (r == RANK_2) {
                    moves.add(create_move(from, to, QUEEN));
                    moves.add(create_move(from, to, ROOK));
                    moves.add(create_move(from, to, BISHOP));
                    moves.add(create_move(from, to, KNIGHT));
                } else {
                    moves.add(create_move(from, to));
                }
            }

            if (b.en_passant != SQ_NONE) {
                U64 ep_mask = pawn_attacks(BLACK, from) & bb_square(b.en_passant);
                if (ep_mask) moves.add(create_ep_move(from, b.en_passant));
            }
        }
    }

    U64 knights = (curr == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    while (knights) {
        Square from = __builtin_ctzll(knights);
        knights &= knights - 1;
        push_moves_from_mask(moves, from, knight_attacks(from) & ~curr_occupied);
    }

    U64 bishops = (curr == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    while (bishops) {
        Square from = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        push_moves_from_mask(moves, from, bishop_attacks(from, b.occupied) & ~curr_occupied);
    }

    U64 rooks = (curr == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    while (rooks) {
        Square from = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        push_moves_from_mask(moves, from, rook_attacks(from, b.occupied) & ~curr_occupied);
    }

    U64 queens = (curr == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];
    while (queens) {
        Square from = __builtin_ctzll(queens);
        queens &= queens - 1;
        push_moves_from_mask(moves, from, queen_attacks(from, b.occupied) & ~curr_occupied);
    }

    Square ksq = king_square(b, curr);
    push_moves_from_mask(moves, ksq, king_attacks(ksq) & ~curr_occupied);

    static constexpr Square E1 = make_square(FILE_E, RANK_1);
    static constexpr Square E8 = make_square(FILE_E, RANK_8);
    static constexpr Square G1 = make_square(FILE_G, RANK_1);
    static constexpr Square C1 = make_square(FILE_C, RANK_1);
    static constexpr Square G8 = make_square(FILE_G, RANK_8);
    static constexpr Square C8 = make_square(FILE_C, RANK_8);

    static constexpr U64 WK_EMPTY = bb_square(make_square(FILE_F, RANK_1)) | bb_square(make_square(FILE_G, RANK_1));
    static constexpr U64 WQ_EMPTY = bb_square(make_square(FILE_B, RANK_1)) | bb_square(make_square(FILE_C, RANK_1)) | bb_square(make_square(FILE_D, RANK_1));
    static constexpr U64 BK_EMPTY = bb_square(make_square(FILE_F, RANK_8)) | bb_square(make_square(FILE_G, RANK_8));
    static constexpr U64 BQ_EMPTY = bb_square(make_square(FILE_B, RANK_8)) | bb_square(make_square(FILE_C, RANK_8)) | bb_square(make_square(FILE_D, RANK_8));

    if (curr == WHITE) {
        if ((b.castling & WHITE_KINGSIDE) && !(b.occupied & WK_EMPTY)
            && (b.bit_boards[WR] & bb_square(make_square(FILE_H, RANK_1)))
            && !is_square_attacked(b, E1, other)
            && !is_square_attacked(b, make_square(FILE_F, RANK_1), other)
            && !is_square_attacked(b, G1, other))
            moves.add(create_move(E1, G1));

        if ((b.castling & WHITE_QUEENSIDE) && !(b.occupied & WQ_EMPTY)
            && (b.bit_boards[WR] & bb_square(make_square(FILE_A, RANK_1)))
            && !is_square_attacked(b, E1, other)
            && !is_square_attacked(b, make_square(FILE_D, RANK_1), other)
            && !is_square_attacked(b, C1, other))
            moves.add(create_move(E1, C1));
    } else {
        if ((b.castling & BLACK_KINGSIDE) && !(b.occupied & BK_EMPTY)
            && (b.bit_boards[BR] & bb_square(make_square(FILE_H, RANK_8)))
            && !is_square_attacked(b, E8, other)
            && !is_square_attacked(b, make_square(FILE_F, RANK_8), other)
            && !is_square_attacked(b, G8, other))
            moves.add(create_move(E8, G8));

        if ((b.castling & BLACK_QUEENSIDE) && !(b.occupied & BQ_EMPTY)
            && (b.bit_boards[BR] & bb_square(make_square(FILE_A, RANK_8)))
            && !is_square_attacked(b, E8, other)
            && !is_square_attacked(b, make_square(FILE_D, RANK_8), other)
            && !is_square_attacked(b, C8, other))
            moves.add(create_move(E8, C8));
    }

    return moves;
}

// Generates pseudo-legal captures and promotions only (used by qsearch when not in check).
MoveList generate_pseudo_legal_captures(Board &b) {
    MoveList moves;
    Colour curr  = b.side_to_move;
    Colour other = flip(curr);

    U64 other_occupied = b.occupancies[other];

    if (curr == WHITE) {
        U64 pawns = b.bit_boards[WP];
        while (pawns) {
            Square from = __builtin_ctzll(pawns);
            pawns &= pawns - 1;
            int r = get_rank(from);

            // Promotions (push to 8th rank)
            if (r == RANK_7) {
                Square to_one = from + 8;
                if (bb_square(to_one) & ~b.occupied) {
                    moves.add(create_move(from, to_one, QUEEN));
                    moves.add(create_move(from, to_one, ROOK));
                    moves.add(create_move(from, to_one, BISHOP));
                    moves.add(create_move(from, to_one, KNIGHT));
                }
            }

            // Pawn captures (including promotion captures)
            U64 captures = pawn_attacks(WHITE, from) & other_occupied;
            while (captures) {
                Square to = __builtin_ctzll(captures);
                captures &= captures - 1;
                if (r == RANK_7) {
                    moves.add(create_move(from, to, QUEEN));
                    moves.add(create_move(from, to, ROOK));
                    moves.add(create_move(from, to, BISHOP));
                    moves.add(create_move(from, to, KNIGHT));
                } else {
                    moves.add(create_move(from, to));
                }
            }

            // En-passant
            if (b.en_passant != SQ_NONE) {
                U64 ep_mask = pawn_attacks(WHITE, from) & bb_square(b.en_passant);
                if (ep_mask) moves.add(create_ep_move(from, b.en_passant));
            }
        }
    } else {
        U64 pawns = b.bit_boards[BP];
        while (pawns) {
            Square from = __builtin_ctzll(pawns);
            pawns &= pawns - 1;
            int r = get_rank(from);

            // Promotions
            if (r == RANK_2) {
                Square to_one = from - 8;
                if (bb_square(to_one) & ~b.occupied) {
                    moves.add(create_move(from, to_one, QUEEN));
                    moves.add(create_move(from, to_one, ROOK));
                    moves.add(create_move(from, to_one, BISHOP));
                    moves.add(create_move(from, to_one, KNIGHT));
                }
            }

            // Pawn captures
            U64 captures = pawn_attacks(BLACK, from) & other_occupied;
            while (captures) {
                Square to = __builtin_ctzll(captures);
                captures &= captures - 1;
                if (r == RANK_2) {
                    moves.add(create_move(from, to, QUEEN));
                    moves.add(create_move(from, to, ROOK));
                    moves.add(create_move(from, to, BISHOP));
                    moves.add(create_move(from, to, KNIGHT));
                } else {
                    moves.add(create_move(from, to));
                }
            }

            // En-passant
            if (b.en_passant != SQ_NONE) {
                U64 ep_mask = pawn_attacks(BLACK, from) & bb_square(b.en_passant);
                if (ep_mask) moves.add(create_ep_move(from, b.en_passant));
            }
        }
    }

    U64 knights = (curr == WHITE) ? b.bit_boards[WN] : b.bit_boards[BN];
    while (knights) {
        Square from = __builtin_ctzll(knights);
        knights &= knights - 1;
        push_moves_from_mask(moves, from, knight_attacks(from) & other_occupied);
    }

    U64 bishops = (curr == WHITE) ? b.bit_boards[WB] : b.bit_boards[BB];
    while (bishops) {
        Square from = __builtin_ctzll(bishops);
        bishops &= bishops - 1;
        push_moves_from_mask(moves, from, bishop_attacks(from, b.occupied) & other_occupied);
    }

    U64 rooks = (curr == WHITE) ? b.bit_boards[WR] : b.bit_boards[BR];
    while (rooks) {
        Square from = __builtin_ctzll(rooks);
        rooks &= rooks - 1;
        push_moves_from_mask(moves, from, rook_attacks(from, b.occupied) & other_occupied);
    }

    U64 queens = (curr == WHITE) ? b.bit_boards[WQ] : b.bit_boards[BQ];
    while (queens) {
        Square from = __builtin_ctzll(queens);
        queens &= queens - 1;
        push_moves_from_mask(moves, from, queen_attacks(from, b.occupied) & other_occupied);
    }

    Square ksq = king_square(b, curr);
    push_moves_from_mask(moves, ksq, king_attacks(ksq) & other_occupied);

    return moves;
}

MoveList generate_legal_moves(Board &b) {
    MoveList pseudo = generate_pseudo_legal_moves(b);
    MoveList legal;
    for (int i = 0; i < pseudo.count; ++i) {
        Undo u;
        if (make_move(b, pseudo.moves[i], u)) {
            legal.add(pseudo.moves[i]);
            unmake_move(b, pseudo.moves[i], u);
        }
    }
    return legal;
}

} // namespace SHAYVERI
