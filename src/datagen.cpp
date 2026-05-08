#include "datagen.h"

#include "board.h"
#include "make.h"
#include "move_gen.h"

#include <atomic>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace SHAYVERI {

std::atomic<U64> total_positions{0};
std::atomic<U64> total_games{0};

int generate_data(int threads, U64 target_pos, char *output_path) {
    std::string path = std::string(output_path) + "_w0.plain";
    std::ofstream out(path);

    if (!out) {
        std::cerr << "failed to open " << path << "\n";
        return 1;
    }

    std::cerr << "datagen threads=" << threads
              << " target_positions=" << target_pos
              << " output=" << path << "\n";

    std::mt19937_64 rng(0x9e3779b97f4a7c15ULL);

    while (total_positions.load(std::memory_order_relaxed) < target_pos) {
        Board b;
        set_startpos(b);

        bool ok = true;
        for (int plies = 0; plies < 8; ++plies) {
            MoveList moves = generate_legal_moves(b);
            if (moves.count == 0) {
                ok = false;
                break;
            }

            std::uniform_int_distribution<int> dist(0, moves.count - 1);
            Move chosen = moves.moves[dist(rng)];
            Undo u;
            if (!make_move(b, chosen, u)) {
                ok = false;
                break;
            }
        }

        if (!ok) continue;

        out << get_board_fen(b) << " | 0 | 1\n";
        total_positions.fetch_add(1, std::memory_order_relaxed);
        total_games.fetch_add(1, std::memory_order_relaxed);
    }

    return 0;
}

} // namespace SHAYVERI
