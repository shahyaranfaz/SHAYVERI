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
TC_VALUE="${TC_VALUE:-$(tc_value)}"

WORKER_ID="${WORKER_ID:-$(hostname -s 2>/dev/null || hostname)}"
WORKER_ID="${WORKER_ID//[^A-Za-z0-9_.-]/_}"

mkdir -p "$RUN_DIR"/{workers,games,done,failed,logs}

require_file "$BOOK"
require_exe "$FASTCHESS"

printf 'worker=%s\nhost=%s\nstarted=%s\n' \
  "$WORKER_ID" "$(hostname 2>/dev/null || echo unknown)" "$(date '+%Y-%m-%d %H:%M:%S')" \
  > "$RUN_DIR/workers/$WORKER_ID.worker"

LOG="$RUN_DIR/logs/$WORKER_ID.log"
touch "$LOG"

log() {
  printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG"
}

fail() {
  printf '%s\n' "$*" > "$RUN_DIR/failed/$WORKER_ID.failed"
  log "failed: $*"
  exit 1
}

run_tournament() {
  local rounds
  rounds="$(rounds_for_pair_games "$GAMES_PER_PAIR_PER_WORKER")"
  local score_interval
  score_interval="${SCORE_INTERVAL:-$((PAIR_COUNT * GAMES_PER_PAIR_PER_WORKER))}"
  local pgn="$RUN_DIR/games/${WORKER_ID}_tournament.pgn"
  local engine_args=()
  mapfile -t engine_args < <(tournament_engine_args)

  log "start tournament engines=$ENGINE_COUNT pairs=$PAIR_COUNT games_per_pair=$GAMES_PER_PAIR_PER_WORKER rounds=$rounds pgn=$pgn"
  (
    cd "$PIN_ROOT"
    "$FASTCHESS" \
      "${engine_args[@]}" \
      -each proto=uci "tc=$TC_VALUE" "timemargin=$TIMEMARGIN" option.Threads=1 \
      -openings "file=$BOOK" format=epd order=random plies=16 \
      -games 2 -rounds "$rounds" -repeat \
      -concurrency "$CONCURRENCY" \
      -recover \
      -pgnout "file=$pgn" min=true \
      -scoreinterval "$score_interval" \
      -ratinginterval "$RATING_INTERVAL"
  ) >> "$LOG" 2>&1 || fail "fastchess tournament failed"
  if grep -Eiq 'illegal|crash|disconnect|forfeit on time|lost on time|timeout' "$LOG"; then
    fail "fastchess reported illegal move, crash, disconnect, timeout, or time loss"
  fi
  log "finished tournament"
}

run_tournament

printf 'worker=%s\nfinished=%s\n' "$WORKER_ID" "$(date '+%Y-%m-%d %H:%M:%S')" > "$RUN_DIR/done/$WORKER_ID.done"
log "done"
