#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

if [[ -z "$RUN_ID" ]]; then
  RUN_ID="$(current_run_id)"
fi
RUN_DIR="$(run_dir "$RUN_ID")"

if [[ -f "$RUN_DIR/config.env" ]]; then
  # shellcheck disable=SC1090
  source "$RUN_DIR/config.env"
fi

count_files() {
  local dir="$1"
  local glob="$2"
  find "$dir" -maxdepth 1 -type f -name "$glob" 2>/dev/null | wc -l
}

human_size() {
  local path="$1"
  if [[ -f "$path" ]]; then
    du -h "$path" 2>/dev/null | awk '{print $1}'
  else
    printf '0'
  fi
}

pgn_games() {
  local path="$1"
  if [[ -f "$path" ]]; then
    grep -c '^\[Event ' "$path" 2>/dev/null || true
  else
    printf '0'
  fi
}

expected_worker_games=$((PAIR_COUNT * GAMES_PER_PAIR_PER_WORKER))

echo "run_id: $RUN_ID"
echo "run_dir: $RUN_DIR"
echo
printf "%-12s %8s\n" "state" "count"
printf "%-12s %8s\n" "workers" "$(count_files "$RUN_DIR/workers" '*.worker')"
printf "%-12s %8s\n" "done" "$(count_files "$RUN_DIR/done" '*.done')"
printf "%-12s %8s\n" "failed" "$(count_files "$RUN_DIR/failed" '*.failed')"
printf "%-12s %8s\n" "games" "$(count_files "$RUN_DIR/games" '*.pgn')"
echo
printf "%-24s %10s %12s %12s %10s\n" "worker" "pgn_size" "games" "total" "complete"
find "$RUN_DIR/workers" -maxdepth 1 -type f -name '*.worker' -printf '%f\n' 2>/dev/null \
  | sed 's/\.worker$//' \
  | sort \
  | while read -r worker; do
      pgn="$RUN_DIR/games/${worker}_tournament.pgn"
      games="$(pgn_games "$pgn")"
      size="$(human_size "$pgn")"
      pct="$(awk -v g="$games" -v t="$expected_worker_games" 'BEGIN { if (t > 0) printf "%.1f%%", (g/t)*100; else printf "0.0%%" }')"
      printf "%-24s %10s %12s %12s %10s\n" "$worker" "$size" "$games" "$expected_worker_games" "$pct"
    done
