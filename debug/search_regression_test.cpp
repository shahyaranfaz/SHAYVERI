#include "attacks.h"
#include "board.h"
#include "make.h"
#include "nnue.h"
#include "search.h"
#include "tt.h"
#include "tune.h"
#include "types.h"
#include "zobrist.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace SHAYVERI;

namespace {

void expect(bool condition, const char* message) {
    if (condition)
        return;
    std::cerr << message << "\n";
    std::exit(1);
}

Board board_from_fen(const char* fen) {
    Board board;
    expect(set_from_fen(board, fen), "failed to parse search regression FEN");
    return board;
}

void reset_search_state() {
    TT.clear();
    clear_search_histories();
    DefaultSearchContext.stop = false;
    DefaultSearchContext.nodes = 0;
    DefaultSearchContext.node_limit = 0;
}

void test_terminal_positions() {
    reset_search_state();
    Board mate = board_from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    SearchResult result = search(mate, 2, nullptr, 0, {}, nullptr, true);
    expect(result.best_move == MOVE_NONE, "checkmate returned a root move");
    expect(result.score == -Tune::MATE_SCORE, "checkmate returned the wrong score");

    reset_search_state();
    Board mate_at_fifty = board_from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 100 1");
    result = search(mate_at_fifty, 2, nullptr, 0, {}, nullptr, true);
    expect(result.score == -Tune::MATE_SCORE,
           "fifty-move handling overrode checkmate at the root");

    reset_search_state();
    Board stalemate = board_from_fen("7k/5Q2/7K/8/8/8/8/8 b - - 0 1");
    result = search(stalemate, 2, nullptr, 0, {}, nullptr, true);
    expect(result.best_move == MOVE_NONE, "stalemate returned a root move");
    expect(result.score == 0, "stalemate returned a non-draw score");
}

void test_draw_rules() {
    reset_search_state();
    Board fifty_move = board_from_fen("7k/8/8/8/8/8/R7/K7 w - - 100 1");
    SearchResult result = search(fifty_move, 2, nullptr, 0, {}, nullptr, true);
    expect(result.score == 0, "fifty-move position returned a non-draw score");

    reset_search_state();
    Board insufficient = board_from_fen("7k/8/8/8/8/8/8/K7 w - - 0 1");
    result = search(insufficient, 2, nullptr, 0, {}, nullptr, true);
    expect(result.score == 0, "insufficient material returned a non-draw score");

    reset_search_state();
    Board repetition = board_from_fen(
        "7k/8/8/8/8/8/Q5N1/6K1 w - - 4 3");
    const Move repeated_move = uci_to_move(repetition, "g2f4");
    expect(repeated_move != MOVE_NONE, "failed to parse repetition test move");
    SearchResult baseline = search(
        repetition, 1, nullptr, 0, {repeated_move}, nullptr, true);
    expect(baseline.score != 0, "repetition baseline unexpectedly scored as a draw");

    Board repeated_child = repetition;
    Undo undo;
    expect(make_move(repeated_child, repeated_move, undo),
           "failed to build repetition child position");
    const U64 history[] = {repetition.hash, repeated_child.hash, repetition.hash};

    reset_search_state();
    result = search(repetition, 1, history, 3, {repeated_move}, nullptr, true);
    expect(result.score == 0, "repeated position returned a non-draw score");
}

void test_checked_qsearch() {
    reset_search_state();
    Board mate = board_from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    expect(qsearch_score(mate) == -Tune::MATE_SCORE,
           "qsearch did not recognize checkmate");

    reset_search_state();
    Board evasion = board_from_fen("7k/8/8/8/8/8/7r/7K w - - 0 1");
    expect(qsearch_score(evasion) > -Tune::MATE_SCORE + Tune::MAX_PLY,
           "qsearch treated a position with a legal evasion as mate");
}

void test_searchmoves_and_node_limit() {
    reset_search_state();
    Board board;
    expect(set_startpos(board), "failed to initialize start position");
    const Move forced = uci_to_move(board, "e2e4");
    expect(forced != MOVE_NONE, "failed to parse forced root move");
    SearchResult result = search(board, 2, nullptr, 0, {forced}, nullptr, true);
    expect(result.best_move == forced, "searchmoves restriction was not respected");

    reset_search_state();
    expect(set_startpos(board), "failed to reset start position");
    result = search_nodes(board, 500, nullptr, 0, {});
    expect(result.nodes == 500, "fixed-node search missed its exact node limit");
    expect(result.best_move != MOVE_NONE, "fixed-node search returned no move");
    Board after = board;
    Undo undo;
    expect(make_move(after, result.best_move, undo),
           "fixed-node search returned an illegal move");
}

void test_search_preserves_root_board() {
    reset_search_state();
    Board board = board_from_fen(
        "r3k2r/p1ppqpb1/bn2pnp1/2pP4/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 0 1");
    const std::string before_fen = get_board_fen(board);
    const U64 before_hash = board.hash;
    const U64 before_pawn_hash = board.pawn_hash;

    const SearchResult result = search(board, 5, nullptr, 0, {}, nullptr, true);
    expect(result.best_move != MOVE_NONE, "middlegame search returned no move");
    expect(get_board_fen(board) == before_fen,
           "search did not restore the root board state");
    expect(board.hash == before_hash, "search did not restore the root hash");
    expect(board.pawn_hash == before_pawn_hash,
           "search did not restore the root pawn hash");
}

void test_iteration_callback() {
    reset_search_state();
    Board board;
    expect(set_startpos(board), "failed to initialize callback start position");

    std::vector<int> callback_depths;
    std::vector<Move> callback_moves;
    std::vector<double> node_fractions;
    IterCallback callback = [&](int depth, Move best_move, int, U64, I64,
                                double best_move_node_fraction) {
        callback_depths.push_back(depth);
        callback_moves.push_back(best_move);
        node_fractions.push_back(best_move_node_fraction);
    };

    const SearchResult result = search(board, 4, nullptr, 0, {}, callback);
    expect(result.depth == 4 && callback_depths.size() == 4,
           "iteration callback did not report every completed depth");
    for (int i = 0; i < 4; ++i) {
        expect(callback_depths[i] == i + 1,
               "iteration callback depths are not sequential");
        expect(node_fractions[i] > 0.0 && node_fractions[i] <= 1.0,
               "best-root-move node fraction is outside (0, 1]");
    }
    expect(callback_moves.back() == result.best_move,
           "final callback best move differs from the search result");
    expect(result.nodes > 0, "search callback test visited no nodes");
}

void test_singular_search_decisions() {
    using SearchDetail::classify_singular_search;

    const int singular_beta       = 100;
    const int beta                = 150;
    const int saved_multicut      = Tune::se_multicut;
    const int saved_negative      = Tune::se_negative_extensions;
    const int saved_negative_tt   = Tune::se_negative_tt_extension;
    const int saved_negative_cut  = Tune::se_negative_cutnode_extension;
    const int saved_double        = Tune::se_double_extensions;
    const int saved_double_margin = Tune::se_double_margin;
    const int saved_double_amount = Tune::se_double_extension;
    const int saved_triple        = Tune::se_triple_extensions;
    const int saved_triple_margin = Tune::se_triple_margin;
    const int saved_triple_amount = Tune::se_triple_extension;

    Tune::se_multicut                   =  1;
    Tune::se_negative_extensions        =  1;
    Tune::se_negative_tt_extension      = -3;
    Tune::se_negative_cutnode_extension = -1;
    Tune::se_double_extensions          =  1;
    Tune::se_double_margin              = 100;
    Tune::se_double_extension           =  2;
    Tune::se_triple_extensions          =  1;
    Tune::se_triple_margin              = 200;
    Tune::se_triple_extension           =  3;

    expect(classify_singular_search(-101, singular_beta, beta, 180, false).extension
               == Tune::se_triple_extension,
           "deep singular fail-low did not select the triple extension");
    expect(classify_singular_search(-1, singular_beta, beta, 180, false).extension
               == Tune::se_double_extension,
           "singular fail-low did not select the double extension");
    expect(classify_singular_search(99, singular_beta, beta, 180, false).extension
               == Tune::se_extension,
           "shallow singular fail-low did not select the base extension");

    const auto multicut = classify_singular_search(150, singular_beta, beta, 180, false);
    expect(multicut.multicut && multicut.extension == 0,
           "excluded-move fail-high did not select multicut");
    expect(!classify_singular_search(
                Tune::MATE_SCORE - 1, singular_beta, beta, 180, false).multicut,
           "mate-range excluded-move score incorrectly selected multicut");

    expect(classify_singular_search(120, singular_beta, beta, 180, false).extension
               == Tune::se_negative_tt_extension,
           "TT fail-high did not select its negative extension");
    expect(classify_singular_search(120, singular_beta, beta, 140, true).extension
               == Tune::se_negative_cutnode_extension,
           "cut node did not select its negative extension");

    Tune::se_double_extensions = 0;
    Tune::se_triple_extensions = 1;
    Tune::se_triple_margin = 50;
    expect(classify_singular_search(40, singular_beta, beta, 180, false).extension
               == Tune::se_triple_extension,
           "disabled double extensions constrained the triple margin");
    Tune::se_double_extensions = 1;
    Tune::se_triple_extensions = 1;
    Tune::se_triple_margin = 200;

    Tune::se_multicut = 0;
    Tune::se_negative_extensions = 0;
    Tune::se_double_extensions = 0;
    Tune::se_triple_extensions = 0;

    expect(!classify_singular_search(150, singular_beta, beta, 180, false).multicut,
           "disabled multicut still produced a cutoff");
    expect(classify_singular_search(120, singular_beta, beta, 180, true).extension == 0,
           "disabled negative extensions still reduced the TT move");
    expect(classify_singular_search(-101, singular_beta, beta, 180, false).extension
               == Tune::se_extension,
           "disabled large extensions did not fall back to the base extension");

    Tune::se_multicut                   = saved_multicut;
    Tune::se_negative_extensions        = saved_negative;
    Tune::se_negative_tt_extension      = saved_negative_tt;
    Tune::se_negative_cutnode_extension = saved_negative_cut;
    Tune::se_double_extensions          = saved_double;
    Tune::se_double_margin              = saved_double_margin;
    Tune::se_double_extension           = saved_double_amount;
    Tune::se_triple_extensions          = saved_triple;
    Tune::se_triple_margin              = saved_triple_margin;
    Tune::se_triple_extension           = saved_triple_amount;
}

} // namespace

int main() {
    std::cout << std::unitbuf;
    Zobrist::init();
    init_attacks();
    TT.resize(16);
    NNUE::set_enabled(false);

    test_terminal_positions();
    std::cout << "[PASS] terminal positions\n";
    test_draw_rules();
    std::cout << "[PASS] draw rules\n";
    test_checked_qsearch();
    std::cout << "[PASS] checked qsearch\n";
    test_searchmoves_and_node_limit();
    std::cout << "[PASS] searchmoves and node limit\n";
    test_search_preserves_root_board();
    std::cout << "[PASS] root board restoration\n";
    test_iteration_callback();
    std::cout << "[PASS] iteration callback\n";
    test_singular_search_decisions();
    std::cout << "[PASS] singular search decisions\n";

    std::cout << "search regression tests passed\n";
    return 0;
}
