#ifndef TYPES_H
#define TYPES_H

#include <cstdint>

namespace SHAYVERI {

using I8  = std::int8_t;
using I16 = std::int16_t;
using I32 = std::int32_t;
using I64 = std::int64_t;

using U8  = std::uint8_t;
using U16 = std::uint16_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;

using Square = int;

constexpr Square SQ_NONE = -1;

enum Colour : int { NONE_COLOUR = -1, WHITE, BLACK, COLOUR_COUNT };

enum Piece : int {
    NONE_PIECE = 0,
    WP, WN, WB, WR, WQ, WK,
    BP, BN, BB, BR, BQ, BK,
    PIECE_COUNT
};

enum PieceType : int { NONE_PTYPE = 0, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

enum File : int { FILE_A = 0, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_COUNT };

enum Rank : int { RANK_1 = 0, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_COUNT };

constexpr Colour   flip(Colour c)    { return c == NONE_COLOUR ? NONE_COLOUR : Colour(c ^ 1); }
constexpr PieceType get_type(Piece p) { return PieceType(p > WK ? p - 6 : p); }
constexpr Colour get_colour(Piece p) {
    return p == NONE_PIECE ? NONE_COLOUR : Colour(p > WK);
}

constexpr Square make_square(File f, Rank r) { return Square(int(r) * 8 + int(f)); }
constexpr File   get_file(Square s)          { return File(s & 7); }
constexpr Rank   get_rank(Square s)          { return Rank(s >> 3); }
constexpr U64    bb_square(Square s)         { return 1ULL << s; }
constexpr bool   is_valid(Square s)          { return s >= 0 && s < 64; }

inline Square pop_lsb(U64 &bb) {
    Square s = __builtin_ctzll(bb);
    bb &= bb - 1;
    return s;
}

inline Piece piece_from_fen_char(char c) {
    switch (c) {
        case 'P': return WP;  case 'N': return WN;  case 'B': return WB;
        case 'R': return WR;  case 'Q': return WQ;  case 'K': return WK;
        case 'p': return BP;  case 'n': return BN;  case 'b': return BB;
        case 'r': return BR;  case 'q': return BQ;  case 'k': return BK;
        default: return NONE_PIECE;
    }
}

} // namespace SHAYVERI

#endif // TYPES_H
