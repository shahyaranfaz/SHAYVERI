#ifndef DATAGEN_H
#define DATAGEN_H

#include "types.h"

#include <string>

namespace SHAYVERI {

struct DatagenOptions {
    int threads = 1;
    U64 target_positions = 0;
    U64 target_games = 0;
    std::string output_prefix;
    std::string output_format = "shayveri-plain-v1";
    std::string eval_file;
    U64 search_nodes = 10000;

    int opening_min_plies = 8;
    int opening_max_plies = 8;
    double book_move_probability = 0.85;
    std::string start_file;
    double start_file_probability = 0.0;
    U64 seed = 0x9e3779b97f4a7c15ULL;

    int max_abs_cp = 3000;
    bool include_checks = false;
    bool include_captures = false;
    bool include_mate_scores = false;
    int min_ply = 0;
    int max_ply = 0;
    int sample_stride = 1;
    int max_samples_per_game = 0;
    bool include_duplicates = true;

    bool enable_adjudication = true;
    int adjudication_cp = 2000;
    int adjudication_plies = 4;
    U64 print_interval = 10000;
};

int generate_data(const DatagenOptions &options);

} // namespace SHAYVERI

#endif // DATAGEN_H
