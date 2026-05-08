#include "board.h"

#include "zobrist.h"

#include <sstream>
#include <string>

#include <cctype>

namespace SHAYVERI {

bool set_from_fen(Board &b, const std::string &fen) {
    b.clear();

    std::istringstream iss(fen);
    std::string board_str, side_str, castle, ep_str, half, full;
    if (!(iss >> board_str >> side_str >> castle >> ep_str >> half >> full))
        return false;

    // Board placement
    int r = 7, f = 0;
    for (char c : board_str) {
        if (c == '/') { r--; f = 0; continue; }
        if (std::isdigit(static_cast<unsigned char>(c))) { f += (c - '0'); continue; }

        Piece p = piece_from_fen_char(c);
        if (p == NONE_PIECE) return false;
        if (f > 7 || r < 0 || r > 7) return false;

        Square sq = make_square(File(f), Rank(r));
        b.bit_boards[p] |= bb_square(sq);
        f++;
    }

    // Side to move
    if (side_str == "w")       b.side_to_move = WHITE;
    else if (side_str == "b")  b.side_to_move = BLACK;
    else                       return false;

    // Castling rights
    b.castling = 0;
    if (castle != "-") {
        for (char c : castle) {
            if      (c == 'K') b.castling |= WHITE_KINGSIDE;
            else if (c == 'Q') b.castling |= WHITE_QUEENSIDE;
            else if (c == 'k') b.castling |= BLACK_KINGSIDE;
            else if (c == 'q') b.castling |= BLACK_QUEENSIDE;
            else               return false;
        }
    }

    // En-passant square
    if (ep_str == "-") {
        b.en_passant = SQ_NONE;
    } else {
        if (ep_str.size() != 2) return false;
        char fc = ep_str[0], rc = ep_str[1];
        if (fc < 'a' || fc > 'h' || rc < '1' || rc > '8') return false;
        b.en_passant = make_square(File(fc - 'a'), Rank(rc - '1'));
    }

    // Half-move clock and full-move number
    try {
        b.half_move = std::stoi(half);
        b.full_move = std::stoi(full);
    } catch (...) { return false; }

    b.recompute_all();
    b.hash = Zobrist::compute(b);
    return true;
}

} // namespace SHAYVERI
