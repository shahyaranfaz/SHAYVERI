#include "evaluate.h"
#include "types.h"


// Piece values (centipawns)
static constexpr int PIECE_VALUES[PIECE_COUNT] = {
    0,    // NONE_PIECE
    100,  // WP
    320,  // WN
    330,  // WB
    500,  // WR
    900,  // WQ
    0,    // WK
   -100,  // BP
   -320,  // BN
   -330,  // BB
   -500,  // BR
   -900,  // BQ
    0,    // BK
};

// Piece-square tables (white's perspective, rank 1 = index 0..7)
// CPW Simplified Evaluation Function by Tomasz Michniewski
static constexpr int PST_PAWN[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,  // rank 1
     5, 10, 10,-20,-20, 10, 10,  5,  // rank 2
     5, -5,-10,  0,  0,-10, -5,  5,  // rank 3
     0,  0,  0, 20, 20,  0,  0,  0,  // rank 4
     5,  5, 10, 25, 25, 10,  5,  5,  // rank 5
    10, 10, 20, 30, 30, 20, 10, 10,  // rank 6
    50, 50, 50, 50, 50, 50, 50, 50,  // rank 7
     0,  0,  0,  0,  0,  0,  0,  0,  // rank 8
};

static constexpr int PST_KNIGHT[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,  // rank 1
    -40,-20,  0,  5,  5,  0,-20,-40,  // rank 2
    -30,  5, 10, 15, 15, 10,  5,-30,  // rank 3
    -30,  0, 15, 20, 20, 15,  0,-30,  // rank 4
    -30,  5, 15, 20, 20, 15,  5,-30,  // rank 5
    -30,  0, 10, 15, 15, 10,  0,-30,  // rank 6
    -40,-20,  0,  0,  0,  0,-20,-40,  // rank 7
    -50,-40,-30,-30,-30,-30,-40,-50,  // rank 8
};

static constexpr int PST_BISHOP[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,  // rank 1
    -10,  5,  0,  0,  0,  0,  5,-10,  // rank 2
    -10, 10, 10, 10, 10, 10, 10,-10,  // rank 3
    -10,  0, 10, 10, 10, 10,  0,-10,  // rank 4
    -10,  5,  5, 10, 10,  5,  5,-10,  // rank 5
    -10,  0,  5, 10, 10,  5,  0,-10,  // rank 6
    -10,  0,  0,  0,  0,  0,  0,-10,  // rank 7
    -20,-10,-10,-10,-10,-10,-10,-20,  // rank 8
};

static constexpr int PST_ROOK[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,  // rank 1
    -5,  0,  0,  0,  0,  0,  0, -5,  // rank 2
    -5,  0,  0,  0,  0,  0,  0, -5,  // rank 3
    -5,  0,  0,  0,  0,  0,  0, -5,  // rank 4
    -5,  0,  0,  0,  0,  0,  0, -5,  // rank 5
    -5,  0,  0,  0,  0,  0,  0, -5,  // rank 6
     5, 10, 10, 10, 10, 10, 10,  5,  // rank 7
     0,  0,  0,  0,  0,  0,  0,  0,  // rank 8
};

static constexpr int PST_QUEEN[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,  // rank 1
    -10,  0,  5,  0,  0,  0,  0,-10,  // rank 2
    -10,  5,  5,  5,  5,  5,  0,-10,  // rank 3
      0,  0,  5,  5,  5,  5,  0, -5,  // rank 4
     -5,  0,  5,  5,  5,  5,  0, -5,  // rank 5
    -10,  0,  5,  5,  5,  5,  0,-10,  // rank 6
    -10,  0,  0,  0,  0,  0,  0,-10,  // rank 7
    -20,-10,-10, -5, -5,-10,-10,-20,  // rank 8
};

static constexpr int PST_KING_MG[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,  // rank 1
     20, 20,  0,  0,  0,  0, 20, 20,  // rank 2
    -10,-20,-20,-20,-20,-20,-20,-10,  // rank 3
    -20,-30,-30,-40,-40,-30,-30,-20,  // rank 4
    -30,-40,-40,-50,-50,-40,-40,-30,  // rank 5
    -30,-40,-40,-50,-50,-40,-40,-30,  // rank 6
    -30,-40,-40,-50,-50,-40,-40,-30,  // rank 7
    -30,-40,-40,-50,-50,-40,-40,-30,  // rank 8
};

static constexpr int PST_KING_EG[64] = {
    -50,-30,-30,-30,-30,-30,-30,-50,  // rank 1
    -30,-30,  0,  0,  0,  0,-30,-30,  // rank 2
    -30,-10, 20, 30, 30, 20,-10,-30,  // rank 3
    -30,-10, 30, 40, 40, 30,-10,-30,  // rank 4
    -30,-10, 30, 40, 40, 30,-10,-30,  // rank 5
    -30,-10, 20, 30, 30, 20,-10,-30,  // rank 6
    -30,-20,-10,  0,  0,-10,-20,-30,  // rank 7
    -50,-40,-30,-20,-20,-30,-40,-50,  // rank 8
};

static constexpr int mirror(int sq) { return (7 - sq / 8) * 8 + (sq % 8); }

static bool is_endgame(const Board &b) {
    bool wq = b.bit_boards[WQ] != 0;
    bool bq = b.bit_boards[BQ] != 0;
    if (!wq && !bq) return true;
    int w_minors = __builtin_popcountll(b.bit_boards[WN] | b.bit_boards[WB]);
    int b_minors = __builtin_popcountll(b.bit_boards[BN] | b.bit_boards[BB]);
    if (wq && w_minors <= 1 && !b.bit_boards[WR]) return true;
    if (bq && b_minors <= 1 && !b.bit_boards[BR]) return true;
    return false;
}

static int pst_white(PieceType pt, int sq, bool endgame) {
    switch (pt) {
        case PAWN: return PST_PAWN[sq];
        case KNIGHT: return PST_KNIGHT[sq];
        case BISHOP: return PST_BISHOP[sq];
        case ROOK: return PST_ROOK[sq];
        case QUEEN: return PST_QUEEN[sq];
        case KING: return endgame ? PST_KING_EG[sq] : PST_KING_MG[sq];
        default: return 0;
    }
}

int evaluate(Board &b) {
    bool eg = is_endgame(b);
    int score = 0;
    for (int p = 1; p < PIECE_COUNT; ++p) {
        U64 bb = b.bit_boards[p];
        Colour c = get_colour(Piece(p));
        PieceType pt = get_type(Piece(p));

        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            int mat = PIECE_VALUES[p];
            int pst = (c == WHITE) ? pst_white(pt, sq, eg) : -pst_white(pt, mirror(sq), eg);
            score += mat + pst;
        }
    }
    if (__builtin_popcountll(b.bit_boards[WB]) >= 2) score += 30;
    if (__builtin_popcountll(b.bit_boards[BB]) >= 2) score -= 30;
    return b.side_to_move == WHITE ? score : -score;
}