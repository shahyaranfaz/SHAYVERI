#include "board.h"

#include <cctype>
#include <sstream>
#include <string>

bool set_from_fen(Board &b, const std::string &fen) {
    b.clear();

    std::istringstream iss(fen);
    std::string board, side_to_move, castle, en_passant, half, full;
    if (!(iss >> board >> side_to_move >> castle >> en_passant >> half >> full))
        return false;

    // board
    int r = 7;
    int f = 0;
    for (char c : board) {
        if (c == '/') { r--; f = 0; continue; }
        if (std::isdigit((unsigned char)c)) { f += (c - '0'); continue; }

        Piece p = piece_from_fen_char(c);
        if (p == NONE_PIECE) return false;
        if (f > 7 || r < 0 || r > 7) return false;

        Square sq = make_square(File(f), Rank(r));
        b.bit_boards[p] |= bb_square(sq);
        f++;
    }

    // side to move
    if (side_to_move == "w")
        b.side_to_move = WHITE;
    else if (side_to_move == "b")
        b.side_to_move = BLACK;
    else
        return false;

    // castling
    b.castling = 0;
    if (castle != "-") {
        for (char c : castle) {
            if (c == 'K')
                b.castling |= WHITE_KINGSIDE;
            else if (c == 'Q')
                b.castling |= WHITE_QUEENSIDE;
            else if (c == 'k')
                b.castling |= BLACK_KINGSIDE;
            else if (c == 'q')
                b.castling |= BLACK_QUEENSIDE;
            else
                return false;
        }
    }

    // en passant
    if (en_passant == "-")
        b.en_passant = SQ_NONE;
    else {
        if (en_passant.size() != 2) return false;
        char file_c = en_passant[0], rank_c = en_passant[1];
        if (file_c < 'a' || file_c > 'h') return false;
        if (rank_c < '1' || rank_c > '8') return false;
        b.en_passant = make_square(File(file_c - 'a'), Rank(rank_c - '1'));
    }

    // half and full move
    try {
        b.half_move = std::stoi(half);
        b.full_move = std::stoi(full);
    } catch (...) { return false; }

    b.recompute_all();
    return true;
}