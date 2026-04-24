#include "zobrist.h"

#include <random>

namespace Zobrist {
    U64 pieces[PIECE_COUNT][64];
    U64 sides;
    U64 castlings[16];
    U64 en_passants[8];

    void init() {
        std::mt19937_64 rng(0xdeadbeefcafe1234ULL); // fixed seed for reproducibility

        for (int p = 0; p < PIECE_COUNT; ++p)
            for (int s = 0; s < 64; ++s)
                pieces[p][s] = rng();

        sides = rng();

        for (int c = 0; c < 16; ++c)
            castlings[c] = rng();

        for (int f = 0; f < 8; ++f)
            en_passants[f] = rng();
    }

    U64 compute(const Board& b) {
        U64 h = 0;

        for (int p = 1; p < PIECE_COUNT; ++p) {
            U64 bb = b.bit_boards[p];
            while (bb) {
                int sq = __builtin_ctzll(bb);
                bb &= bb - 1;
                h ^= pieces[p][sq];
            }
        }

        if (b.side_to_move == BLACK)
            h ^= sides;

        h ^= castlings[b.castling & 15];

        if (b.en_passant != SQ_NONE)
            h ^= en_passants[get_file(b.en_passant)];

        return h;
    }
}
