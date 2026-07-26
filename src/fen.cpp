#include "board.h"

#include "zobrist.h"

#include <cassert>
#include <sstream>
#include <string>

namespace SHAYVERI {

bool set_from_fen(Board &b, const std::string &fen) {
    std::istringstream iss(fen);
    std::string board_str, side_str, castle, ep_str;
    int half_move = 0;
    int full_move = 0;
    if (!(iss >> board_str >> side_str >> castle >> ep_str >> half_move >> full_move) ||
        half_move < 0 || full_move < 1)
        return false;
    std::string trailing;
    if (iss >> trailing) return false;

    Board parsed;
    parsed.clear();

    int rank = 7;
    int file = 0;
    for (char c : board_str) {
        if (c == '/') {
            if (file != 8 || rank == 0) return false;
            --rank;
            file = 0;
            continue;
        }
        if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) return false;
            continue;
        }

        const Piece piece = piece_from_fen_char(c);
        if (piece == NONE_PIECE || file >= 8) return false;

        const Square square = make_square(File(file), Rank(rank));
        parsed.bit_boards[piece] |= bb_square(square);
        ++file;
    }
    if (rank != 0 || file != 8) return false;

    if (side_str == "w")       parsed.side_to_move = WHITE;
    else if (side_str == "b")  parsed.side_to_move = BLACK;
    else                       return false;

    if (castle != "-") {
        for (char c : castle) {
            int right = 0;
            if      (c == 'K') right = WHITE_KINGSIDE;
            else if (c == 'Q') right = WHITE_QUEENSIDE;
            else if (c == 'k') right = BLACK_KINGSIDE;
            else if (c == 'q') right = BLACK_QUEENSIDE;
            else               return false;
            if (parsed.castling & right) return false;
            parsed.castling |= right;
        }
    }

    if (ep_str == "-") {
        parsed.en_passant = SQ_NONE;
    } else {
        if (ep_str.size() != 2) return false;
        const char fc = ep_str[0];
        const char rc = ep_str[1];
        if (fc < 'a' || fc > 'h' || rc < '1' || rc > '8') return false;
        parsed.en_passant = make_square(File(fc - 'a'), Rank(rc - '1'));
    }

    parsed.half_move = half_move;
    parsed.full_move = full_move;
    parsed.recompute_all();

    // Preserve the invariants required by move generation and NNUE evaluation.
    // Promotions replace pawns, so no legally reachable position can contain
    // more than the initial 32 pieces.
    if (!parsed.is_plausible_position())
        return false;

    parsed.hash = Zobrist::compute(parsed);
    parsed.pawn_hash = Zobrist::compute_pawns(parsed);
    assert(parsed.is_consistent());
    b = parsed;
    return true;
}

} // namespace SHAYVERI
