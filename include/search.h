#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"
#include "move_io.h"
#include "tt.h"

#include <atomic>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace SHAYVERI {

class SearchWorker;
struct SearchRequest;
struct SearchResult;

struct SearchContext {
    explicit SearchContext(TranspositionTable &transposition_table);
    ~SearchContext();

    SearchContext(const SearchContext &) = delete;
    SearchContext &operator=(const SearchContext &) = delete;

    void clear_histories();

    TranspositionTable &table;
    std::atomic<bool> stop{false};
    std::atomic<U64> nodes{0};
    std::atomic<U64> node_limit{0};

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    friend SearchResult search(
        SearchContext &, SearchWorker &, Board &, int,
        const SearchRequest &);
    friend SearchResult search_nodes(
        SearchContext &, SearchWorker &, Board &, U64,
        const SearchRequest &);
};

class SearchWorker {
public:
    SearchWorker();
    ~SearchWorker();

    SearchWorker(SearchWorker &&) noexcept;
    SearchWorker &operator=(SearchWorker &&) noexcept;

    SearchWorker(const SearchWorker &) = delete;
    SearchWorker &operator=(const SearchWorker &) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    friend SearchResult search(
        SearchContext &, SearchWorker &, Board &, int,
        const SearchRequest &);
    friend SearchResult search_nodes(
        SearchContext &, SearchWorker &, Board &, U64,
        const SearchRequest &);
    friend int qsearch_score(SearchContext &, SearchWorker &, Board &);
};

// depth, best move, score, total nodes, elapsed ms, best-root-move node share
using IterCallback = std::function<void(int, Move, int, U64, I64, double)>;

struct SearchRequest {
    std::span<const U64> repetition{};
    std::span<const Move> root_moves{};
    IterCallback on_iteration{};
    bool emit_info = false;
    bool retain_history = false;
    int root_bias = 0;
};

struct SearchResult {
    Move best_move = MOVE_NONE;
    Move ponder_move = MOVE_NONE;
    int score = 0;
    int depth = 0;
    int selective_depth = 0;
    U64 nodes = 0;
};

namespace SearchDetail {

struct SingularSearchDecision {
    int extension = 0;
    bool multicut = false;
};

SingularSearchDecision classify_singular_search(
    int singular_score, int singular_beta, int beta, int tt_score, bool cut_node);

I16 clamp_history_value(int value);
I16 gravity_history_update(I16 entry, int bonus, int history_max);

} // namespace SearchDetail

SearchResult search(SearchContext &context,
                    SearchWorker &worker,
                    Board &b, int max_depth,
                    const SearchRequest &request);

SearchResult search_nodes(SearchContext &context,
                          SearchWorker &worker,
                          Board &b, U64 max_nodes,
                          const SearchRequest &request);

int qsearch_score(SearchContext &context, SearchWorker &worker, Board &b);

} // namespace SHAYVERI

#endif // SEARCH_H
