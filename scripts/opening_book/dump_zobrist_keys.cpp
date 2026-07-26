#include "zobrist.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace SHAYVERI;

// Minimal JSON writer (no extra deps)
// Writes schema expected by build_source.py:
// {
//   "pieces": [[...64...], ... 13 ...],
//   "sides": <u64>,
//   "castlings": [...16...],
//   "en_passants": [...8...]
// }
static void write_u64(std::ostream& os, std::uint64_t v) {
    os << v; // Python int() reads the decimal representation directly.
}

int main(int argc, char** argv) {
    std::string out_path = "outputs/zobrist_keys.json";
    if (argc >= 2) out_path = argv[1];

    Zobrist::init();

    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output: " << out_path << "\n";
        return 1;
    }

    out << "{\n";

    // pieces
    out << "  \"pieces\": [\n";
    for (int p = 0; p < PIECE_COUNT; ++p) {
        out << "    [";
        for (int sq = 0; sq < 64; ++sq) {
            write_u64(out, static_cast<std::uint64_t>(Zobrist::pieces[p][sq]));
            if (sq != 63) out << ", ";
        }
        out << "]";
        if (p != PIECE_COUNT - 1) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    // sides
    out << "  \"sides\": ";
    write_u64(out, static_cast<std::uint64_t>(Zobrist::sides));
    out << ",\n";

    // castlings
    out << "  \"castlings\": [";
    for (int i = 0; i < 16; ++i) {
        write_u64(out, static_cast<std::uint64_t>(Zobrist::castlings[i]));
        if (i != 15) out << ", ";
    }
    out << "],\n";

    // en_passants
    out << "  \"en_passants\": [";
    for (int i = 0; i < 8; ++i) {
        write_u64(out, static_cast<std::uint64_t>(Zobrist::en_passants[i]));
        if (i != 7) out << ", ";
    }
    out << "]\n";

    out << "}\n";

    out.flush();
    std::cout << "Wrote " << out_path << "\n";
    return 0;
}
