#include "datagen_cli.h"

#include "parse_cli.h"

#include <iostream>
#include <string>

namespace SHAYVERI::DatagenCLI {

namespace {

bool require_value(
    int argc, char **argv, int &index, std::string &value) {
    if (index + 1 >= argc) return false;
    value = argv[++index];
    return true;
}

} // namespace

bool parse_args(int argc, char **argv, DatagenOptions &options) {
    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        std::string value;

        auto need_int = [&](int &out) {
            return require_value(argc, argv, i, value)
                && ParseCLI::integer(value, out);
        };
        auto need_u64 = [&](U64 &out) {
            return require_value(argc, argv, i, value)
                && ParseCLI::unsigned_integer(value, out);
        };
        auto need_double = [&](double &out) {
            return require_value(argc, argv, i, value)
                && ParseCLI::real(value, out);
        };
        auto need_bool = [&](bool &out) {
            return require_value(argc, argv, i, value)
                && ParseCLI::boolean(value, out);
        };

        if (key == "--threads") {
            if (!need_int(options.threads)) return false;
        } else if (key == "--positions") {
            if (!need_u64(options.target_positions)) return false;
        } else if (key == "--games") {
            if (!need_u64(options.target_games)) return false;
        } else if (key == "--output-prefix") {
            if (!require_value(argc, argv, i, options.output_prefix))
                return false;
        } else if (key == "--output-format") {
            if (!require_value(argc, argv, i, options.output_format))
                return false;
        } else if (key == "--eval-file") {
            if (!require_value(argc, argv, i, options.eval_file))
                return false;
        } else if (key == "--nodes") {
            if (!need_u64(options.search_nodes)) return false;
        } else if (key == "--opening-min-plies") {
            if (!need_int(options.opening_min_plies)) return false;
        } else if (key == "--opening-max-plies") {
            if (!need_int(options.opening_max_plies)) return false;
        } else if (key == "--book-prob") {
            if (!need_double(options.book_move_probability)) return false;
        } else if (key == "--start-file") {
            if (!require_value(argc, argv, i, options.start_file))
                return false;
        } else if (key == "--start-file-prob") {
            if (!need_double(options.start_file_probability)) return false;
        } else if (key == "--seed") {
            if (!need_u64(options.seed)) return false;
        } else if (key == "--max-abs-cp") {
            if (!need_int(options.max_abs_cp)) return false;
        } else if (key == "--include-checks") {
            if (!need_bool(options.include_checks)) return false;
        } else if (key == "--include-captures") {
            if (!need_bool(options.include_captures)) return false;
        } else if (key == "--include-mate-scores") {
            if (!need_bool(options.include_mate_scores)) return false;
        } else if (key == "--include-duplicates") {
            if (!need_bool(options.include_duplicates)) return false;
        } else if (key == "--min-ply") {
            if (!need_int(options.min_ply)) return false;
        } else if (key == "--max-ply") {
            if (!need_int(options.max_ply)) return false;
        } else if (key == "--sample-stride") {
            if (!need_int(options.sample_stride)) return false;
        } else if (key == "--max-samples-per-game") {
            if (!need_int(options.max_samples_per_game)) return false;
        } else if (key == "--enable-adjudication") {
            if (!need_bool(options.enable_adjudication)) return false;
        } else if (key == "--adjudication-cp") {
            if (!need_int(options.adjudication_cp)) return false;
        } else if (key == "--adjudication-plies") {
            if (!need_int(options.adjudication_plies)) return false;
        } else if (key == "--print-interval") {
            if (!need_u64(options.print_interval)) return false;
        } else {
            return false;
        }
    }
    return true;
}

void print_usage(const char *argv0) {
    std::cerr
        << "usage: " << argv0 << " datagen"
        << " --threads <n>"
        << " [--positions <n>]"
        << " [--games <n>]"
        << " --output-prefix <path>"
        << " [--output-format shayveri-plain-v1|bullet-v1]"
        << " [--eval-file <path|<embedded>|<hce>>]"
        << " [--nodes <n>]"
        << " [--opening-min-plies <n>]"
        << " [--opening-max-plies <n>]"
        << " [--book-prob <0..1>]"
        << " [--start-file <fen-or-epd-file>]"
        << " [--start-file-prob <0..1>]"
        << " [--seed <n>]"
        << " [--max-abs-cp <n>]"
        << " [--include-checks <bool>]"
        << " [--include-captures <bool>]"
        << " [--include-mate-scores <bool>]"
        << " [--include-duplicates <bool>]"
        << " [--min-ply <n>]"
        << " [--max-ply <n>]"
        << " [--sample-stride <n>]"
        << " [--max-samples-per-game <n>]"
        << " [--enable-adjudication <bool>]"
        << " [--adjudication-cp <n>]"
        << " [--adjudication-plies <n>]"
        << " [--print-interval <games>]\n";
}

} // namespace SHAYVERI::DatagenCLI
