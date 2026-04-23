#ifndef BOARD_H
#define BOARD_H

#include "types.h"

#include <array>
#include <string>

enum Castling : int  {
    WHITE_KINGSIDE = 1 << 0,
    WHITE_QUEENSIDE = 1 << 1,
    BLACK_KINGSIDE = 1 << 2,
    BLACK_QUEENSIDE = 1 << 3
};

struct Board {
    std::array<U64, PIECE_COUNT> bit_boards{}; // per-piece bitboards
    std::array<U64, COLOUR_COUNT> occupancies{}; // occupancy by color
    std::array<Piece, 64> mailbox{}; // for O(1) square lookups

    U64 hash = 0;
    U64 occupied = 0;
    Colour side_to_move = WHITE;
    Square en_passant = SQ_NONE;
    int castling = 0;
    int half_move = 0;
    int full_move = 1;

    void clear();
    void recompute_all();

    Piece get_piece(Square s) const;
};

bool set_from_fen(Board &b, const std::string &fen);

inline bool set_startpos(Board &b) { return set_from_fen(b, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"); }

#endif