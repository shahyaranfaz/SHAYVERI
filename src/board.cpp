#include "board.h"

#include "zobrist.h"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace SHAYVERI {

void Board::clear() {
    bit_boards.fill(0);
    occupancies.fill(0);
    mailbox.fill(NONE_PIECE);
    hash         = 0;
    pawn_hash    = 0;
    occupied     = 0;
    side_to_move = WHITE;
    castling     = 0;
    en_passant   = SQ_NONE;
    half_move    = 0;
    full_move    = 1;
}

void Board::recompute_all() {
    occupancies[WHITE] = bit_boards[WP] | bit_boards[WN] | bit_boards[WB]
                       | bit_boards[WR] | bit_boards[WQ] | bit_boards[WK];
    occupancies[BLACK] = bit_boards[BP] | bit_boards[BN] | bit_boards[BB]
                       | bit_boards[BR] | bit_boards[BQ] | bit_boards[BK];
    occupied = occupancies[WHITE] | occupancies[BLACK];

    mailbox.fill(NONE_PIECE);
    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = bit_boards[p];
        while (bb) {
            Square s = pop_lsb(bb);
            mailbox[s] = Piece(p);
        }
    }
}

Piece Board::get_piece(Square s) const {
    return mailbox[s];
}

bool Board::is_consistent() const {
    if (side_to_move != WHITE && side_to_move != BLACK) return false;
    if (castling < 0 || castling > 15) return false;
    if (en_passant != SQ_NONE && !is_valid(en_passant)) return false;

    U64 white = 0;
    U64 black = 0;
    U64 all = 0;
    for (int p = 1; p < PIECE_COUNT; ++p) {
        const U64 pieces = bit_boards[p];
        if (all & pieces) return false;
        all |= pieces;
        if (get_colour(Piece(p)) == WHITE) white |= pieces;
        else black |= pieces;
    }
    if (occupancies[WHITE] != white || occupancies[BLACK] != black
        || occupied != all || (white & black))
        return false;

    for (int sq = 0; sq < 64; ++sq) {
        Piece expected = NONE_PIECE;
        for (int p = 1; p < PIECE_COUNT; ++p) {
            if (bit_boards[p] & bb_square(Square(sq))) {
                expected = Piece(p);
                break;
            }
        }
        if (mailbox[sq] != expected) return false;
    }

    return hash == Zobrist::compute(*this)
        && pawn_hash == Zobrist::compute_pawns(*this);
}

bool Board::is_plausible_position() const {
    if (__builtin_popcountll(bit_boards[WK]) != 1
        || __builtin_popcountll(bit_boards[BK]) != 1
        || __builtin_popcountll(occupied) > 32
        || half_move < 0 || full_move < 1)
        return false;

    constexpr U64 back_ranks = 0xFF000000000000FFULL;
    if ((bit_boards[WP] | bit_boards[BP]) & back_ranks)
        return false;
    if (__builtin_popcountll(bit_boards[WP]) > 8
        || __builtin_popcountll(bit_boards[BP]) > 8
        || __builtin_popcountll(occupancies[WHITE]) > 16
        || __builtin_popcountll(occupancies[BLACK]) > 16)
        return false;

    const Square white_king = __builtin_ctzll(bit_boards[WK]);
    const Square black_king = __builtin_ctzll(bit_boards[BK]);
    const int king_file_distance =
        std::abs(static_cast<int>(get_file(white_king))
                 - static_cast<int>(get_file(black_king)));
    const int king_rank_distance =
        std::abs(static_cast<int>(get_rank(white_king))
                 - static_cast<int>(get_rank(black_king)));
    if (king_file_distance <= 1 && king_rank_distance <= 1)
        return false;

    const Square e1 = make_square(FILE_E, RANK_1);
    const Square e8 = make_square(FILE_E, RANK_8);
    if ((castling & (WHITE_KINGSIDE | WHITE_QUEENSIDE))
            && get_piece(e1) != WK)
        return false;
    if ((castling & (BLACK_KINGSIDE | BLACK_QUEENSIDE))
            && get_piece(e8) != BK)
        return false;
    if ((castling & WHITE_KINGSIDE)
            && get_piece(make_square(FILE_H, RANK_1)) != WR)
        return false;
    if ((castling & WHITE_QUEENSIDE)
            && get_piece(make_square(FILE_A, RANK_1)) != WR)
        return false;
    if ((castling & BLACK_KINGSIDE)
            && get_piece(make_square(FILE_H, RANK_8)) != BR)
        return false;
    if ((castling & BLACK_QUEENSIDE)
            && get_piece(make_square(FILE_A, RANK_8)) != BR)
        return false;

    if (en_passant != SQ_NONE) {
        const Rank expected_rank = side_to_move == WHITE ? RANK_6 : RANK_3;
        if (get_rank(en_passant) != expected_rank
            || get_piece(en_passant) != NONE_PIECE)
            return false;
        const Square pawn_square = side_to_move == WHITE
            ? en_passant - 8 : en_passant + 8;
        const Piece expected_pawn = side_to_move == WHITE ? BP : WP;
        if (!is_valid(pawn_square) || get_piece(pawn_square) != expected_pawn)
            return false;
        const Square origin_square = side_to_move == WHITE
            ? en_passant + 8 : en_passant - 8;
        if (!is_valid(origin_square)
            || get_piece(origin_square) != NONE_PIECE)
            return false;
    }
    return true;
}

std::string square_to_coord(Square s) {
    if (s == SQ_NONE) return "-";

    std::string coord = "";
    coord += static_cast<char>('a' + (s % 8));
    coord += static_cast<char>('1' + (s / 8));
    return coord;
}

std::string get_board_fen(const Board &b) {
    std::ostringstream fen;

    // Piece Placement (Rank 8 down to Rank 1)
    for (int rank = 7; rank >= 0; --rank) {
        int empty_count = 0;
        for (int file = 0; file < 8; ++file) {
            Square sq = static_cast<Square>(rank * 8 + file);
            Piece pc = b.get_piece(sq);

            if (pc == NONE_PIECE) {
                empty_count++;
            } else {
                if (empty_count > 0) {
                    fen << empty_count;
                    empty_count = 0;
                }
                // Mapping Piece enum to FEN chars
                static const char piece_to_char[] = ".PNBRQKpnbrqk";
                fen << piece_to_char[pc];
            }
        }
        if (empty_count > 0) fen << empty_count;
        if (rank > 0) fen << '/';
    }

    // Side to move
    fen << (b.side_to_move == WHITE ? " w " : " b ");

    // Castling Rights
    if (b.castling == 0) {
        fen << "-";
    } else {
        if (b.castling & WHITE_KINGSIDE)  fen << 'K';
        if (b.castling & WHITE_QUEENSIDE) fen << 'Q';
        if (b.castling & BLACK_KINGSIDE)  fen << 'k';
        if (b.castling & BLACK_QUEENSIDE) fen << 'q';
    }

    // En Passant Square
    fen << " " << square_to_coord(b.en_passant);

    // Halfmove Clock and Fullmove Number
    fen << " " << b.half_move << " " << b.full_move;

    return fen.str();
}

} // namespace SHAYVERI
