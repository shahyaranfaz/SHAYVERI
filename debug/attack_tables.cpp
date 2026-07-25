#include "attacks.h"
#include "types.h"

#include <iostream>

using namespace SHAYVERI;

namespace {

constexpr int FILE_STEP[8] = {0, 0, 1, -1, 1, -1, 1, -1};
constexpr int RANK_STEP[8] = {1, -1, 0, 0, 1, 1, -1, -1};

bool valid(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

U64 ray_attacks(Square square, U64 occupied, bool bishop) {
    const int start = bishop ? 4 : 0;
    const int end = bishop ? 8 : 4;
    U64 attacks = 0;
    for (int direction = start; direction < end; ++direction) {
        int file = get_file(square) + FILE_STEP[direction];
        int rank = get_rank(square) + RANK_STEP[direction];
        while (valid(file, rank)) {
            const U64 bit = bb_square(make_square(File(file), Rank(rank)));
            attacks |= bit;
            if (occupied & bit) break;
            file += FILE_STEP[direction];
            rank += RANK_STEP[direction];
        }
    }
    return attacks;
}

U64 full_mask(Square square, bool bishop) {
    return ray_attacks(square, 0, bishop);
}

bool verify_slider(bool bishop) {
    for (int index = 0; index < 64; ++index) {
        const Square square = Square(index);
        const U64 mask = full_mask(square, bishop);
        for (U64 occupied = 0;; occupied = (occupied - mask) & mask) {
            const U64 expected = ray_attacks(square, occupied, bishop);
            const U64 actual = bishop
                ? bishop_attacks(square, occupied)
                : rook_attacks(square, occupied);
            if (actual != expected) {
                std::cerr << "attack mismatch: " << (bishop ? "bishop" : "rook")
                          << " square=" << index
                          << " occupied=" << occupied << "\n";
                return false;
            }
            if (occupied == mask) break;
        }
    }
    return true;
}

} // namespace

int main() {
    init_attacks();
    if (!verify_slider(true) || !verify_slider(false)) return 2;
    std::cout << "Sliding attack tables match exhaustive ray generation\n";
    return 0;
}
