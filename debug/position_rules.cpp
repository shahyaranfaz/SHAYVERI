#include "attacks.h"
#include "board.h"
#include "position_rules.h"
#include "zobrist.h"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace SHAYVERI;

namespace {

void expect(bool condition, const char *message) {
    if (condition)
        return;
    std::cerr << message << "\n";
    std::exit(1);
}

Board board_from_fen(const char *fen) {
    Board board;
    expect(set_from_fen(board, fen), "failed to parse position-rules FEN");
    return board;
}

void test_check_state() {
    const Board checked =
        board_from_fen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");
    expect(PositionRules::is_in_check(checked),
           "failed to recognize check");

    const Board quiet =
        board_from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    expect(!PositionRules::is_in_check(quiet),
           "quiet position was reported as check");
}

void test_insufficient_material() {
    expect(PositionRules::has_insufficient_material(
               board_from_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1")),
           "king versus king was not insufficient");
    expect(PositionRules::has_insufficient_material(
               board_from_fen("4k3/8/8/8/8/8/8/2B1K3 w - - 0 1")),
           "king and bishop versus king was not insufficient");
    expect(PositionRules::has_insufficient_material(
               board_from_fen("4k3/8/6b1/8/4B3/8/8/4K3 w - - 0 1")),
           "same-coloured bishops were not insufficient");

    expect(!PositionRules::has_insufficient_material(
               board_from_fen("4k3/8/5b2/8/4B3/8/8/4K3 w - - 0 1")),
           "opposite-coloured bishops were reported insufficient");
    expect(!PositionRules::has_insufficient_material(
               board_from_fen("4k3/8/6n1/8/4N3/8/8/4K3 w - - 0 1")),
           "two knights were reported insufficient");
    expect(!PositionRules::has_insufficient_material(
               board_from_fen("4k3/8/8/8/8/8/P7/4K3 w - - 0 1")),
           "pawn material was reported insufficient");
}

void test_repetition_semantics() {
    constexpr U64 key = 0x1234;
    const U64 search_history[] = {key, 7, key, 8, 9};
    expect(PositionRules::has_search_repetition(
               key, search_history, 4, 4),
           "search repetition missed a same-side prior occurrence");
    expect(!PositionRules::has_search_repetition(
               key, search_history, 4, 1),
           "search repetition crossed the reversible-move window");

    const std::vector<U64> twice{key, 7, key};
    const std::vector<U64> three_times{key, 7, key, 8, key};
    expect(!PositionRules::is_threefold_repetition(twice, key),
           "two occurrences were treated as a threefold repetition");
    expect(PositionRules::is_threefold_repetition(three_times, key),
           "threefold repetition was not recognized");
}

} // namespace

int main() {
    Zobrist::init();
    init_attacks();

    test_check_state();
    test_insufficient_material();
    test_repetition_semantics();

    std::cout << "Position-rules suite passed\n";
    return 0;
}
