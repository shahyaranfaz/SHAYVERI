#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

TC_VALUE="$(tc_value)"
TC_LABEL="$(tc_label)"
RUN_ID="${NAME_ID}_${TC_LABEL}_$(date +%Y%m%d_%H%M%S)"
RUN_DIR="$(run_dir "$RUN_ID")"

mkdir -p "$PIN_ROOT/results" "$RUN_DIR"/{workers,games,done,failed,state,logs}
printf '%s\n' "$RUN_ID" > "$PIN_ROOT/current_run_id"

LOG="$RUN_DIR/master.log"
touch "$LOG"

log() {
  printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG"
}

write_config() {
  : > "$RUN_DIR/config.env"
  write_env PIN_ROOT "$PIN_ROOT"
  write_env SCRIPT_DIR "$SCRIPT_DIR"
  write_env RUN_ID "$RUN_ID"
  write_env ENGINES_DIR "$ENGINES_DIR"
  write_env BOOK "$BOOK"
  write_env ANCHORS "$ANCHORS"
  write_env ORDO "$ORDO"
  write_env CUTECHESS "$CUTECHESS"
  write_env TC "$TC"
  write_env TC_LABEL "$TC_LABEL"
  write_env TC_VALUE "$TC_VALUE"
  write_env TIMEMARGIN "$TIMEMARGIN"
  write_env GAMES_PER_PAIR_PER_WORKER "$GAMES_PER_PAIR_PER_WORKER"
  write_env NAME_ID "$NAME_ID"
  write_env NET "$NET"
  write_env SHAYVERI_OPTIONS "$SHAYVERI_OPTIONS"
  write_env FIXED_OPPONENTS "$FIXED_OPPONENTS"
  write_env ORDO_FLAGS "$ORDO_FLAGS"
}

write_env() {
  local key="$1"
  local value="$2"
  printf '%s=%q\n' "$key" "$value" >> "$RUN_DIR/config.env"
}

write_config

require_file "$BOOK"
require_file "$ANCHORS"
require_exe "$CUTECHESS"
[[ -x "$ORDO" ]] || log "warning: ORDO not executable at $ORDO; master will skip Ordo if still missing at end"
cp "$ANCHORS" "$RUN_DIR/anchors"
if [[ -x "$ORDO" ]]; then
  cp "$ORDO" "$RUN_DIR/ordo"
  chmod +x "$RUN_DIR/ordo"
fi

log "run_id=$RUN_ID"
log "run_dir=$RUN_DIR"
log "registration window=${REGISTER_SECONDS}s"
log "name_id=$NAME_ID net=${NET:-default}"
log "engines=$NAME_ID,$FIXED_OPPONENTS"
log "tc=$TC_VALUE label=$TC_LABEL games_per_pair_per_worker=$GAMES_PER_PAIR_PER_WORKER pair_count=$PAIR_COUNT"

deadline=$((SECONDS + REGISTER_SECONDS))
while (( SECONDS < deadline )); do
  count="$(find "$RUN_DIR/workers" -maxdepth 1 -type f -name '*.worker' 2>/dev/null | wc -l)"
  remaining=$((deadline - SECONDS))
  log "registered_workers=$count remaining=${remaining}s"
  sleep 30
done

mapfile -t workers < <(find "$RUN_DIR/workers" -maxdepth 1 -type f -name '*.worker' -printf '%f\n' 2>/dev/null | sed 's/\.worker$//' | sort)
printf '%s\n' "${workers[@]}" > "$RUN_DIR/state/registered_workers.txt"

if (( ${#workers[@]} == 0 )); then
  log "no workers registered; aborting"
  exit 1
fi

log "registration closed workers=${workers[*]}"
log "waiting for workers to finish"

for worker in "${workers[@]}"; do
  while [[ ! -f "$RUN_DIR/done/$worker.done" && ! -f "$RUN_DIR/failed/$worker.failed" ]]; do
    sleep 20
  done
  if [[ -f "$RUN_DIR/failed/$worker.failed" ]]; then
    log "worker failed: $worker"
    cat "$RUN_DIR/failed/$worker.failed" | tee -a "$LOG"
    exit 1
  fi
  log "worker done: $worker"
done

log "combining pgns"
find "$RUN_DIR/games" -maxdepth 1 -type f -name '*.pgn' | sort | xargs cat > "$RUN_DIR/rating_pool.pgn"

if [[ ! -s "$RUN_DIR/rating_pool.pgn" ]]; then
  log "combined PGN missing/empty"
  exit 1
fi

log "rating_pool=$RUN_DIR/rating_pool.pgn"

if [[ -x "$RUN_DIR/ordo" ]]; then
  log "running ordo"
  (
    cd "$RUN_DIR"
    # shellcheck disable=SC2086
    ./ordo -p rating_pool.pgn $ORDO_FLAGS -o results.txt -c results.csv
  ) | tee "$RUN_DIR/ordo.stdout.txt"
  log "ordo_out=$RUN_DIR/results.txt"
else
  log "skipping ordo; missing executable: $ORDO"
fi

if [[ "${KEEP_WORKER_PGNS:-0}" != "1" ]]; then
  log "removing individual worker PGNs"
  rm -f "$RUN_DIR/games"/*.pgn
fi

log "done"
