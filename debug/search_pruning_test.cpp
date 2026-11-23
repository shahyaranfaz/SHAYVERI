// Exercise the internal null-window search without adding a public engine API.
#include "../src/search.cpp"
#include "zobrist.h"

#include <cstdlib>

using namespace SHAYVERI;

int main(int argc, char **argv) {
    Zobrist::init();
    init_attacks();
    std::string net_error;
    const char *net_path = argc > 1 ? argv[1] : "SHAYVERI2_11_7.nnue";
    if (!SHAYVERI::NNUE::load(net_path, net_error)) {
        std::cerr << "[FAIL] Test network: " << net_error << "\n";
        return 1;
    }

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

    table.clear();
    history->clear();
    Board board;
    if (!set_from_fen(board, "7k/8/8/8/8/4B3/8/K5Q1 w - - 8 1"))
        return EXIT_FAILURE;
    U64 repetition[MAX_PLY * 2]{};
    repetition[0] = PositionRules::repetition_key(board);
    Undo undo;
    make_null_move(board, undo);
    make_legal_move(board, uci_to_move(board, "h8h7"), undo);
    repetition[1] = PositionRules::repetition_key(board);
    make_null_move(board, undo);
    const MoveList legal = generate_legal_moves(board);
    if (legal.count != 1 || move_to_uci(legal.moves[0]) != "h7h8")
        return EXIT_FAILURE;
    stack[2].acc.refresh(board);
    SearchThreadState thread;
    const std::string before = get_board_fen(board);
    const U64 before_key = repetition[0];
    const int score = search_after_null(context, thread, board, 1, -INF, INF,
                                        3, *history, repetition, 2, false,
                                        stack.get() + 2);
    if (get_board_fen(board) != before || repetition[0] != before_key)
        return EXIT_FAILURE;
    if (score >= 0) {
        std::cerr << "[FAIL] Null subtree treated an artificial cycle as a draw: "
                  << score << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Null subtree does not claim repetition across passes\n";
}
