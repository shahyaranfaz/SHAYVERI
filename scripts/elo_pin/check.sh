#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CHECK_ROOT="$(mktemp -d)"
trap 'rm -rf "$CHECK_ROOT"' EXIT

mkdir -p "$CHECK_ROOT"/{books,engines}
: > "$CHECK_ROOT/books/check.epd"
: > "$CHECK_ROOT/anchors"

cat > "$CHECK_ROOT/fastchess" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
pgn=""
rounds=0
while (( $# > 0 )); do
  case "$1" in
    -pgnout) shift; pgn="${1#file=}" ;;
    -rounds) shift; rounds="$1" ;;
  esac
  shift
done
mkdir -p "$(dirname "$pgn")"
games=$((36 * 2 * rounds))
for ((game = 1; game <= games; game++)); do
  printf '[Event "check"]\n[Result "1/2-1/2"]\n\n1. e4 e5 1/2-1/2\n\n'
done > "$pgn"
EOF

cat > "$CHECK_ROOT/ordo" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
results=results.txt
csv=results.csv
h2h=h2h.txt
cfs=cfs.csv
err=err.csv
while (( $# > 0 )); do
  case "$1" in
    -o) shift; results="$1" ;;
    -c) shift; csv="$1" ;;
    -j) shift; h2h="$1" ;;
    -C) shift; cfs="$1" ;;
    -e) shift; err="$1" ;;
  esac
  shift
done
printf 'results\n' > "$results"
printf 'csv\n' > "$csv"
printf 'h2h\n' > "$h2h"
printf 'cfs\n' > "$cfs"
printf 'err\n' > "$err"
printf 'ordo complete\n'
EOF

chmod +x "$CHECK_ROOT/fastchess" "$CHECK_ROOT/ordo"

PIN_ROOT="$CHECK_ROOT" \
BOOK="$CHECK_ROOT/books/check.epd" \
ANCHORS="$CHECK_ROOT/anchors" \
ORDO="$CHECK_ROOT/ordo" \
FASTCHESS="$CHECK_ROOT/fastchess" \
RELEASE_ID=check \
REGISTER_SECONDS=1 \
POLL_SECONDS=1 \
WORKER_ACK_SECONDS=5 \
STC_GAMES_PER_PAIR=4 \
LTC_GAMES_PER_PAIR=4 \
STC_SHARD_PAIR_GAMES=2 \
LTC_SHARD_PAIR_GAMES=2 \
"$SCRIPT_DIR/master.sh" > "$CHECK_ROOT/master.stdout" 2>&1 &
master_pid=$!

for _ in {1..50}; do
  [[ -f "$CHECK_ROOT/current_run_id" ]] && break
  sleep 0.1
done
[[ -f "$CHECK_ROOT/current_run_id" ]] || {
  cat "$CHECK_ROOT/master.stdout"
  exit 1
}

PIN_ROOT="$CHECK_ROOT" \
FASTCHESS="$CHECK_ROOT/fastchess" \
WORKER_ID=check1 \
"$SCRIPT_DIR/worker.sh" > "$CHECK_ROOT/worker1.stdout" 2>&1 &
worker1_pid=$!

PIN_ROOT="$CHECK_ROOT" \
FASTCHESS="$CHECK_ROOT/fastchess" \
WORKER_ID=check2 \
"$SCRIPT_DIR/worker.sh" > "$CHECK_ROOT/worker2.stdout" 2>&1 &
worker2_pid=$!

wait "$master_pid" || {
  cat "$CHECK_ROOT/master.stdout"
  cat "$CHECK_ROOT/worker1.stdout"
  cat "$CHECK_ROOT/worker2.stdout"
  exit 1
}
wait "$worker1_pid"
wait "$worker2_pid"

for phase in stc ltc; do
  for output in rating_pool.pgn results.txt h2h.txt results.csv cfs.csv err.csv ordo.stdout.txt; do
    [[ -s "$CHECK_ROOT/outputs/check/$phase/$output" ]] || {
      echo "missing check output: $phase/$output" >&2
      exit 1
    }
  done
  games="$(grep -c '^\[Event ' "$CHECK_ROOT/outputs/check/$phase/rating_pool.pgn")"
  [[ "$games" == 144 ]] || {
    echo "$phase has $games games, expected 144" >&2
    exit 1
  }
done

[[ ! -e "$CHECK_ROOT/current_run_id" ]]
[[ -z "$(find "$CHECK_ROOT/.work" -mindepth 1 -print -quit)" ]]
echo "elo pin check passed"
