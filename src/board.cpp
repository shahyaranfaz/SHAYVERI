#include "board.h"

#include "zobrist.h"

#include <sstream>
#include <vector>

namespace SHAYVERI {

void Board::clear() {
    bit_boards.fill(0);
    occupancies.fill(0);
    mailbox.fill(NONE_PIECE);
    hash         = 0;
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
