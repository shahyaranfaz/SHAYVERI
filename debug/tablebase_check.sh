#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_DIR/.." && pwd)
BASE_URL=${SYZYGY_BASE_URL:-https://tablebase.sesse.net/syzygy/3-4-5}
CACHE_ROOT=${XDG_CACHE_HOME:-$HOME/.cache}
TB_DIR=${SYZYGY_PATH:-$CACHE_ROOT/shayveri/syzygy/3-4-5}
CXX=${CXX:-g++}

require_command() {
    command -v "$1" >/dev/null 2>&1 || {
        printf 'error: required command not found: %s\n' "$1" >&2
        exit 1
    }
}

require_command curl
require_command md5sum
require_command "$CXX"

mkdir -p "$TB_DIR"

CURL_FLAGS=(
    --fail
    --location
    --retry 5
    --retry-delay 2
    --continue-at -
)

# tablebase.sesse.net currently presents an invalid TLS certificate. Integrity
# is enforced below with the mirror's complete checksum manifest.
if [[ "$BASE_URL" == https://tablebase.sesse.net/* ]]; then
    CURL_FLAGS+=(--insecure)
fi

manifest="$TB_DIR/checksum.md5"
manifest_tmp="$manifest.tmp"
curl "${CURL_FLAGS[@]}" --continue-at 0 \
    --output "$manifest_tmp" "$BASE_URL/checksum.md5"
mv -- "$manifest_tmp" "$manifest"

downloaded=0
skipped=0
while read -r expected filename; do
    [[ -n "${expected:-}" && -n "${filename:-}" ]] || continue
    if [[ ! "$expected" =~ ^[0-9a-fA-F]{32}$ ]] \
        || [[ ! "$filename" =~ ^[A-Za-z0-9+v.-]+\.rtb[wz]$ ]]; then
        printf 'error: unsafe or malformed checksum entry: %s %s\n' \
            "$expected" "$filename" >&2
        exit 1
    fi

    target="$TB_DIR/$filename"
    if [[ -f "$target" ]] \
        && printf '%s  %s\n' "$expected" "$target" | md5sum --check --status; then
        skipped=$((skipped + 1))
        continue
    fi

    # A wrong complete file cannot be resumed. Remove only the validated,
    # filename-scoped target inside TB_DIR before downloading it again.
    if [[ -f "$target" ]]; then
        rm -f -- "$target"
    fi
    printf 'downloading %s\n' "$filename"
    curl "${CURL_FLAGS[@]}" --output "$target" "$BASE_URL/$filename"
    printf '%s  %s\n' "$expected" "$target" | md5sum --check --status || {
        printf 'error: checksum mismatch after download: %s\n' "$filename" >&2
        exit 1
    }
    downloaded=$((downloaded + 1))
done < "$manifest"

(
    cd "$TB_DIR"
    md5sum --check --strict checksum.md5
)

wdl_count=$(find "$TB_DIR" -maxdepth 1 -type f -name '*.rtbw' | wc -l)
dtz_count=$(find "$TB_DIR" -maxdepth 1 -type f -name '*.rtbz' | wc -l)
(( wdl_count > 0 && dtz_count > 0 )) || {
    printf 'error: incomplete tablebase set: WDL=%d DTZ=%d\n' \
        "$wdl_count" "$dtz_count" >&2
    exit 1
}

work_dir=$(mktemp -d)
cleanup() {
    rm -rf -- "$work_dir"
}
trap cleanup EXIT

test_src="$work_dir/tablebase_check.cpp"
test_bin="$work_dir/tablebase_check"

cat > "$test_src" <<'CPP'
#include "attacks.h"
#include "board.h"
#include "move_gen.h"
#include "tablebase.h"
#include "zobrist.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using namespace SHAYVERI;

[[noreturn]] void fail(const std::string &message) {
    std::cerr << "tablebase check failed: " << message << '\n';
    std::exit(1);
}

Board position(const char *fen) {
    Board board;
    if (!set_from_fen(board, fen))
        fail(std::string("invalid test FEN: ") + fen);
    return board;
}

void expect(bool condition, const char *message) {
    if (!condition)
        fail(message);
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2)
        fail("expected the Syzygy directory as the only argument");

    SHAYVERI::init_attacks();
    SHAYVERI::Zobrist::init();

    expect(Tablebase::initialize(argv[1]), "initialization failed");
    expect(Tablebase::max_pieces() == 5,
           "complete 3-4-5 set did not report cardinality 5");

    Board winning = position("7k/8/8/8/8/8/6Q1/6K1 w - - 0 1");
    expect(Tablebase::can_probe(winning), "KQvK was not probeable");
    expect(Tablebase::probe_wdl(winning) == Tablebase::WDL::Win,
           "winning KQvK did not probe as a win");

    const Tablebase::RootResult root = Tablebase::probe_root(winning);
    expect(root.wdl == Tablebase::WDL::Win, "root KQvK was not a win");
    expect(root.move != MOVE_NONE, "root KQvK returned no move");
    expect(is_legal_move(winning, root.move),
           "Fathom root move did not convert to a legal SHAYVERI move");

    Board losing = position("7k/8/8/8/8/8/6Q1/6K1 b - - 0 1");
    expect(Tablebase::probe_wdl(losing) == Tablebase::WDL::Loss,
           "losing KQvK did not probe as a loss");

    Board rule50 = position("7k/8/8/8/8/8/6Q1/6K1 w - - 1 1");
    expect(Tablebase::probe_wdl(rule50) == Tablebase::WDL::Failed,
           "search WDL accepted a nonzero fifty-move counter");
    expect(Tablebase::probe_root(rule50).move != MOVE_NONE,
           "root WDL rejected a legal nonzero fifty-move counter");

    Board castling = position("4k2r/8/8/8/8/8/8/R3K3 w Qk - 0 1");
    expect(!Tablebase::can_probe(castling),
           "position with castling rights was reported probeable");

    Board too_many = position("7k/8/8/8/8/8/PPPP4/6K1 w - - 0 1");
    expect(!Tablebase::can_probe(too_many),
           "position above loaded cardinality was reported probeable");

    Tablebase::shutdown();
    std::cout << "tablebase integration checks passed\n";
}
CPP

"$CXX" \
    -std=c++20 -O2 -Wall -Wextra -Wpedantic -mbmi2 \
    -I"$REPO_ROOT/include" \
    -I"$REPO_ROOT/external/fathom/src" \
    -o "$test_bin" \
    "$test_src" \
    "$REPO_ROOT/src/attacks.cpp" \
    "$REPO_ROOT/src/board.cpp" \
    "$REPO_ROOT/src/fen.cpp" \
    "$REPO_ROOT/src/make.cpp" \
    "$REPO_ROOT/src/move_gen.cpp" \
    "$REPO_ROOT/src/tablebase.cpp" \
    "$REPO_ROOT/src/zobrist.cpp" \
    "$REPO_ROOT/external/fathom/src/tbprobe.c" \
    -lpthread

"$test_bin" "$TB_DIR"

printf 'Syzygy 3-4-5 check complete: path=%s downloaded=%d cached=%d WDL=%d DTZ=%d\n' \
    "$TB_DIR" "$downloaded" "$skipped" "$wdl_count" "$dtz_count"
