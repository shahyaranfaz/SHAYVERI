#include "board.h"

void Board::clear() {
    bit_boards.fill(0);
    occupancies.fill(0);
    mailbox.fill(NONE_PIECE);
    hash = 0;
    occupied = 0;
    side_to_move = WHITE;
    castling = 0;
    en_passant = SQ_NONE;
    half_move = 0;
    full_move = 1;
}

void Board::recompute_all() {
    occupancies[WHITE] = bit_boards[WP] | bit_boards[WN] | bit_boards[WB] | bit_boards[WR] | bit_boards[WQ] | bit_boards[WK];
    occupancies[BLACK] = bit_boards[BP] | bit_boards[BN] | bit_boards[BB] | bit_boards[BR] | bit_boards[BQ] | bit_boards[BK];
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