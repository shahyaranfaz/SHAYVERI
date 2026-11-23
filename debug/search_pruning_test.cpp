// Exercise the internal null-window search without adding a public engine API.
#include "../src/search.cpp"
#include "zobrist.h"

#include <cstdlib>

using namespace SHAYVERI;

int main() {
    Zobrist::init();
    init_attacks();
    NNUE::set_enabled(false);
    TranspositionTable table;
    table.resize(1);
    SearchContext context(table);
    auto stack = std::make_unique<StackInfo[]>(MAX_PLY + 5);
    auto history = std::make_unique<SearchHeuristics>();

    for (int alpha : {-10, -500}) {
        table.clear();
        Board board;
        if (!set_from_fen(board, "5qqk/8/8/8/8/8/8/K7 w - - 0 1")
            || generate_legal_moves(board).count == 0)
            return EXIT_FAILURE;
        stack[2].acc.refresh(board);
        U64 repetition[MAX_PLY * 2]{};
        repetition[0] = board.hash;
        SearchThreadState thread;
        const int score = negamax(context, thread, board, 1, alpha, alpha + 1,
                                  0, *history, repetition, 1, false, false,
                                  stack.get() + 2);
        if (score > alpha) {
            std::cerr << "[FAIL] Pruned losing moves returned " << score
                      << " above alpha=" << alpha << '\n';
            return EXIT_FAILURE;
        }
        if (get_board_fen(board) != "5qqk/8/8/8/8/8/8/K7 w - - 0 1")
            return EXIT_FAILURE;
    }
    std::cout << "[PASS] All-pruned nodes preserve fail-low bounds\n";
}
