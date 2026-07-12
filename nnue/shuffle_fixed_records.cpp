// Fixed-size record shuffler for Bullet data.
//
// Accepts multiple inputs and avoids creating a combined raw file:
//   shuffle_fixed_records --output out.partial.bin --input a.bin --input b.bin
//   shuffle_fixed_records --output out.partial.bin --input-dir shards/
//
// Method matches the previous successful pipeline shape:
//   split into fixed-size chunks, shuffle records inside each chunk, randomize
//   chunk order, concatenate, and delete chunks as they are consumed.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    std::vector<fs::path> inputs;
    std::vector<fs::path> input_dirs;
    fs::path output;
    std::uint64_t record_size = 32;
    std::uint64_t block_records = 4'194'304;
    std::uint64_t limit_records = 0;
    std::uint64_t seed = 0x9e3779b97f4a7c15ULL;
};

[[noreturn]] void fail(const std::string &message) {
    std::cerr << "error: " << message << '\n';
    std::exit(1);
}

std::uint64_t file_size_checked(const fs::path &path) {
    std::error_code ec;
    auto size = fs::file_size(path, ec);
    if (ec)
        fail("failed to stat " + path.string() + ": " + ec.message());
    return static_cast<std::uint64_t>(size);
}

bool ends_with(const std::string &text, const std::string &suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<fs::path> collect_inputs(const Options &options) {
    std::vector<fs::path> inputs = options.inputs;
    for (const auto &dir : options.input_dirs) {
        if (!fs::is_directory(dir))
            fail("missing input dir: " + dir.string());
        for (const auto &entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file())
                continue;
            const std::string path = entry.path().string();
            if (ends_with(path, ".bullet.bin") || ends_with(path, ".bin"))
                inputs.push_back(entry.path());
        }
    }
    std::sort(inputs.begin(), inputs.end());
    inputs.erase(std::unique(inputs.begin(), inputs.end()), inputs.end());
    if (inputs.empty())
        fail("no inputs");
    return inputs;
}

void swap_record(std::vector<char> &data,
                 std::uint64_t record_size,
                 std::uint64_t a,
                 std::uint64_t b,
                 std::vector<char> &scratch) {
    if (a == b)
        return;
    char *pa = data.data() + a * record_size;
    char *pb = data.data() + b * record_size;
    std::copy(pa, pa + record_size, scratch.data());
    std::copy(pb, pb + record_size, pa);
    std::copy(scratch.data(), scratch.data() + record_size, pb);
}

void shuffle_records(std::vector<char> &data, std::uint64_t record_size, std::mt19937_64 &rng) {
    const std::uint64_t count = data.size() / record_size;
    std::vector<char> scratch(record_size);
    for (std::uint64_t i = count; i > 1; --i) {
        std::uniform_int_distribution<std::uint64_t> dist(0, i - 1);
        swap_record(data, record_size, i - 1, dist(rng), scratch);
    }
}

Options parse_args(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](const std::string &name) -> std::string {
            if (i + 1 >= argc)
                fail("missing value for " + name);
            return argv[++i];
        };

        if (arg == "--input") {
            options.inputs.emplace_back(need(arg));
        } else if (arg == "--input-dir") {
            options.input_dirs.emplace_back(need(arg));
        } else if (arg == "--output") {
            options.output = need(arg);
        } else if (arg == "--record-size") {
            options.record_size = std::stoull(need(arg));
        } else if (arg == "--block-records") {
            options.block_records = std::stoull(need(arg));
        } else if (arg == "--limit-records") {
            options.limit_records = std::stoull(need(arg));
        } else if (arg == "--seed") {
            options.seed = std::stoull(need(arg));
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "usage: shuffle_fixed_records --output <file> [--input <file>...] [--input-dir <dir>...]\n"
                << "       [--record-size 32] [--block-records N] [--limit-records N] [--seed N]\n";
            std::exit(0);
        } else {
            fail("unknown argument: " + arg);
        }
    }

    if (options.output.empty())
        fail("--output is required");
    if (options.record_size == 0)
        fail("--record-size must be positive");
    if (options.block_records == 0)
        fail("--block-records must be positive");
    return options;
}

} // namespace

int main(int argc, char **argv) {
    const Options options = parse_args(argc, argv);
    const auto inputs = collect_inputs(options);

    std::uint64_t available_records = 0;
    for (const auto &input : inputs) {
        if (!fs::is_regular_file(input))
            fail("missing input: " + input.string());
        const std::uint64_t size = file_size_checked(input);
        if (size % options.record_size)
            fail("bad record alignment: " + input.string());
        available_records += size / options.record_size;
    }

    const std::uint64_t target_records =
        options.limit_records == 0 ? available_records : options.limit_records;
    if (available_records < target_records)
        fail("only " + std::to_string(available_records) +
             " records available, need " + std::to_string(target_records));

    const fs::path output_parent = options.output.parent_path();
    if (!output_parent.empty())
        fs::create_directories(output_parent);
    const fs::path tmp_dir = options.output.parent_path() / ("." + options.output.filename().string() + ".blocks");
    if (fs::exists(tmp_dir))
        fail("temporary block directory already exists: " + tmp_dir.string());
    fs::create_directories(tmp_dir);

    std::mt19937_64 rng(options.seed);
    std::vector<fs::path> blocks;
    std::vector<char> buffer;
    buffer.reserve(static_cast<size_t>(options.block_records * options.record_size));

    std::uint64_t written_records = 0;
    std::uint64_t block_idx = 0;

    try {
        for (const auto &input : inputs) {
            std::ifstream in(input, std::ios::binary);
            if (!in)
                fail("failed to open input: " + input.string());

            while (written_records < target_records) {
                const std::uint64_t remaining = target_records - written_records;
                const std::uint64_t wanted_records =
                    std::min<std::uint64_t>(options.block_records, remaining);
                const std::uint64_t wanted_bytes = wanted_records * options.record_size;

                buffer.resize(static_cast<size_t>(wanted_bytes));
                in.read(buffer.data(), static_cast<std::streamsize>(wanted_bytes));
                const std::uint64_t got_bytes = static_cast<std::uint64_t>(in.gcount());
                if (got_bytes == 0)
                    break;
                if (got_bytes % options.record_size)
                    fail("partial record read from " + input.string());
                buffer.resize(static_cast<size_t>(got_bytes));

                const std::uint64_t got_records = got_bytes / options.record_size;
                shuffle_records(buffer, options.record_size, rng);

                fs::path block = tmp_dir / ("block_" + std::to_string(block_idx) + ".bin");
                std::ofstream out(block, std::ios::binary);
                if (!out)
                    fail("failed to create block: " + block.string());
                out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                out.close();

                blocks.push_back(block);
                written_records += got_records;
                ++block_idx;
                std::cout << "block " << block_idx << ": " << got_records
                          << " records (" << written_records << " / "
                          << target_records << ")\n";
            }
            if (written_records >= target_records)
                break;
        }

        std::shuffle(blocks.begin(), blocks.end(), rng);
        std::ofstream out(options.output, std::ios::binary);
        if (!out)
            fail("failed to create output: " + options.output.string());

        std::vector<char> copy_buffer(8 * 1024 * 1024);
        for (size_t i = 0; i < blocks.size(); ++i) {
            std::ifstream in(blocks[i], std::ios::binary);
            if (!in)
                fail("failed to read block: " + blocks[i].string());
            while (in) {
                in.read(copy_buffer.data(), static_cast<std::streamsize>(copy_buffer.size()));
                const std::streamsize got = in.gcount();
                if (got > 0)
                    out.write(copy_buffer.data(), got);
            }
            in.close();
            fs::remove(blocks[i]);
            std::cout << "\rcat " << (i + 1) << " / " << blocks.size() << std::flush;
        }
        if (!blocks.empty())
            std::cout << '\n';
        out.close();

        const std::uint64_t expected_bytes = target_records * options.record_size;
        const std::uint64_t output_bytes = file_size_checked(options.output);
        if (output_bytes != expected_bytes)
            fail("output size mismatch: " + std::to_string(output_bytes) +
                 " != " + std::to_string(expected_bytes));

        fs::remove(tmp_dir);
        std::cout << "shuffled " << target_records << " records to "
                  << options.output << '\n';
    } catch (...) {
        for (const auto &block : blocks) {
            std::error_code ec;
            fs::remove(block, ec);
        }
        std::error_code ec;
        fs::remove(options.output, ec);
        fs::remove(tmp_dir, ec);
        throw;
    }

    return 0;
}
