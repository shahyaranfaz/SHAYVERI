// Cover score bounds and actual search entry points without a new public API.
#include "../src/search.cpp"
#include "zobrist.h"

#include <cstdlib>
#include <thread>
#include <vector>

using namespace SHAYVERI;

static void require(bool condition, const char *message) {
    if (!condition) {
        std::cerr << "[FAIL] Syzygy search: " << message << '\n';
        std::exit(1);
    }
}

static Board position(const char *fen) {
    Board board;
    require(set_from_fen(board, fen), "invalid fixture FEN");
    return board;
}

int main(int argc, char **argv) {
    require(argc == 3, "expected network and fixture directory");
    Zobrist::init();
    init_attacks();
    std::string error;
    require(NNUE::load(argv[1], error), "network load failed");
    require(Tablebase::initialize(argv[2]), "tablebase initialization failed");
    require(Tablebase::max_pieces() == 3, "fixture cardinality mismatch");
    TranspositionTable table;
    table.resize(1);
    SearchContext context(table);
    SearchWorker worker;
    auto reset = [&] { table.clear(); context.tb_hits = 0; context.nodes = 0; };

    for (bool winning : {true, false}) {
        reset();
        Board board = position(winning
            ? "7k/8/8/8/8/8/6Q1/6K1 w - - 0 1"
            : "7k/8/8/8/8/8/6Q1/6K1 b - - 0 1");
        const std::string before = get_board_fen(board);
        const auto root = Tablebase::probe_root(board);
        require(root.wdl == (winning ? Tablebase::WDL::Win : Tablebase::WDL::Loss), "root WDL mismatch");
        require(winning ? root.dtz > 0 : root.dtz < 0, "DTZ sign mismatch");
        const auto result = search(context, worker, board, 2, {});
        require(result.best_move == root.move && is_legal_move(board, result.best_move), "search did not use legal root DTZ move");
        require(result.score == (winning ? TB_SCORE : -TB_SCORE), "tablebase win/loss score mismatch");
        require(context.tb_hits > 0, "search reported no tablebase hit");
        require(get_board_fen(board) == before, "root probing changed board state");

        const auto legal = generate_legal_moves(board);
        for (int i = 0; i < legal.count; ++i) {
            if (legal.moves[i] == root.move) continue;
            reset();
            const Move allowed[] = {legal.moves[i]};
            const auto restricted = search(context, worker, board, 1,
                                           SearchRequest{.root_moves = allowed});
            require(restricted.best_move == allowed[0], "DTZ ignored searchmoves");
            break;
        }

        reset();
        auto stack = std::make_unique<StackInfo[]>(MAX_PLY + 5);
        auto history = std::make_unique<SearchHeuristics>();
        stack[2].acc.refresh(board);
        U64 repetition[MAX_PLY * 2]{};
        repetition[0] = PositionRules::repetition_key(board);
        SearchThreadState thread;
        const int score = negamax(context, thread, board, 1, -100, 100,
                                  5, *history, repetition, 1, false, false,
                                  stack.get() + 2);
        require(score == (winning ? TB_SCORE - 5 : -TB_SCORE + 5), "in-search WDL bound was not used");
        require(context.tb_hits == 1, "in-search probe count mismatch");
        if (winning) {
            reset();
            stack[2].excluded_move = root.move;
            negamax(context, thread, board, 1, -INF, INF, 5,
                    *history, repetition, 1, false, false, stack.get() + 2);
            require(context.tb_hits == 0,
                    "full-position WDL result was used in an excluded-move search");
        }
    }

    Board clock = position("7k/8/8/8/8/8/6Q1/6K1 w - - 99 1");
    require(Tablebase::probe_root(clock).wdl == Tablebase::WDL::CursedWin,
            "rule-50 cursed win was not classified");
    Board blessed = position("7k/8/8/8/8/8/6Q1/6K1 b - - 99 1");
    require(Tablebase::probe_root(blessed).wdl == Tablebase::WDL::BlessedLoss,
            "rule-50 blessed loss was not classified");
    reset();
    require(search(context, worker, clock, 1, {}).score == 0, "root DTZ ignored imminent fifty-move draw");
    require(Tablebase::probe_wdl(clock) == Tablebase::WDL::Failed, "WDL ignored nonzero rule-50 clock");
    reset();
    table.store(clock.hash, 10, TB_SCORE, TT_EXACT, MOVE_NONE);
    auto clock_stack = std::make_unique<StackInfo[]>(MAX_PLY + 5);
    auto clock_history = std::make_unique<SearchHeuristics>();
    clock_stack[2].acc.refresh(clock);
    U64 clock_repetition[MAX_PLY * 2]{};
    clock_repetition[0] = PositionRules::repetition_key(clock);
    SearchThreadState clock_thread;
    require(negamax(context, clock_thread, clock, 1, -INF, INF, 0,
                    *clock_history, clock_repetition, 1, false, false,
                    clock_stack.get() + 2) == 0,
            "cached clock-zero WDL score overrode fifty-move draw");
    Board mate = position("7k/6Q1/6K1/8/8/8/8/8 b - - 100 1");
    reset();
    require(search(context, worker, mate, 1, {}).score == -MATE_SCORE, "tablebase overrode checkmate");
    Board stale = position("7k/5Q2/7K/8/8/8/8/8 b - - 0 1");
    reset();
    require(search(context, worker, stale, 1, {}).score == 0, "tablebase overrode stalemate");

    Board promotion = position("k7/5K1P/8/8/8/8/8/8 w - - 0 1");
    const auto promoted = Tablebase::probe_root(promotion);
    require(promoted.wdl == Tablebase::WDL::Win && move_promo(promoted.move) != NONE_PTYPE
            && is_legal_move(promotion, promoted.move), "root promotion was not converted correctly");
    Board captured_draw = position("7k/7R/5K2/8/8/8/8/8 b - - 0 1");
    reset();
    const auto draw = search(context, worker, captured_draw, 1, {});
    require(draw.score == 0 && move_to_uci(draw.best_move) == "h8h7",
            "root did not preserve a draw through capture into bare kings");

    Board transition = position("k7/6r1/8/8/8/8/6Q1/6K1 w - - 0 1");
    reset();
    const Move capture[] = {uci_to_move(transition, "g2g7")};
    require(capture[0] != MOVE_NONE, "invalid transition capture fixture");
    const auto entered = search(context, worker, transition, 1,
                                SearchRequest{.root_moves = capture});
    require(entered.best_move == capture[0] && context.tb_hits > 0,
            "capture from outside tablebase coverage did not reach WDL probing");

    Board concurrent = position("7k/8/8/8/8/8/6Q1/6K1 w - - 0 1");
    std::atomic<bool> valid{true};
    std::vector<std::thread> probes;
    for (int i = 0; i < 4; ++i) {
        probes.emplace_back([&] {
            for (int j = 0; j < 20; ++j) {
                if (Tablebase::probe_wdl(concurrent) != Tablebase::WDL::Win
                    || Tablebase::probe_root(concurrent).wdl != Tablebase::WDL::Win)
                    valid = false;
            }
        });
    }
    for (auto &thread : probes) thread.join();
    require(valid, "concurrent root/WDL probes failed");

    for (int score : {TB_SCORE - 5, -TB_SCORE + 5, MATE_SCORE - 5, -MATE_SCORE + 5}) {
        const int expected = score > 0 ? score - 6 : score + 6;
        require(score_from_tt(score_to_tt(score, 5), 11) == expected, "decisive TT score did not normalize ply");
    }
    require(score_from_tt(score_to_tt(123, 5), 11) == 123, "ordinary TT score changed with ply");

    require(Tablebase::initialize(std::string(argv[2]) + "/wdl_only"), "WDL-only fixture load failed");
    Board win = position("7k/8/8/8/8/8/6Q1/6K1 w - - 0 1");
    require(Tablebase::probe_root(win).wdl == Tablebase::WDL::Win, "WDL-only root fallback failed");
    require(Tablebase::probe_root(clock).wdl == Tablebase::WDL::Failed, "WDL fallback invented a nonzero-clock DTZ result");
    Tablebase::shutdown();
    require(!Tablebase::can_probe(win), "shutdown left probing enabled");
    std::cout << "[PASS] Syzygy root/search integration, score domains, restrictions, rule-50, and WDL-only fallback\n";
}
