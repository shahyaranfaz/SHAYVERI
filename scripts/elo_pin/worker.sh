#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

[[ -n "$RUN_ID" ]] || RUN_ID="$(current_run_id)"
WORK_ROOT="$(run_work_dir "$RUN_ID")"
[[ -f "$WORK_ROOT/config.env" ]] || die "missing run configuration: $WORK_ROOT/config.env"
# shellcheck disable=SC1090
source "$WORK_ROOT/config.env"

WORKER_ID="${WORKER_ID:-$(hostname -s 2>/dev/null || hostname)}"
WORKER_ID="${WORKER_ID//[^A-Za-z0-9_.-]/_}"
LOG="$WORK_ROOT/logs/$WORKER_ID.log"
CURRENT_JOB=""
CURRENT_QUEUE=""
CURRENT_PGN=""

mkdir -p "$WORK_ROOT"/{workers,failed,logs}
touch "$LOG"

log() {
  printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG"
}

return_claim() {
  if [[ -n "$CURRENT_JOB" && -f "$CURRENT_JOB" && -n "$CURRENT_QUEUE" ]]; then
    mv "$CURRENT_JOB" "$CURRENT_QUEUE" 2>/dev/null || true
  fi
  [[ -z "$CURRENT_PGN" ]] || rm -f -- "$CURRENT_PGN"
}

trap return_claim EXIT
trap 'return_claim; exit 130' INT TERM

require_file "$BOOK"
require_exe "$FASTCHESS"

printf 'worker=%s\nhost=%s\nregistered=%s\n' \
  "$WORKER_ID" "$(hostname 2>/dev/null || echo unknown)" "$(date '+%Y-%m-%d %H:%M:%S')" \
  > "$WORK_ROOT/workers/$WORKER_ID.worker"
log "registered run=$RUN_ID"

while [[ ! -f "$WORK_ROOT/state/registration.closed" ]]; do
  sleep "$POLL_SECONDS"
done

grep -Fxq "$WORKER_ID" "$WORK_ROOT/state/registered_workers.txt" || \
  die "worker $WORKER_ID was not included in the frozen roster"

claim_job() {
  local phase_root="$1"
  local queued
  local base
  local claimed

  for queued in "$phase_root/queue"/*.job; do
    [[ -e "$queued" ]] || return 1
    base="$(basename "$queued" .job)"
    claimed="$phase_root/working/${base}.${WORKER_ID}.job"
    if mv "$queued" "$claimed" 2>/dev/null; then
      CURRENT_JOB="$claimed"
      CURRENT_QUEUE="$phase_root/queue/${base}.job"
      return 0
    fi
  done
  return 1
}

run_shard() {
  local phase_root="$1"
  local job="$2"
  local engine_args=()
  local expected_games
  local actual_games
  local final_pgn
  local shard_log
  local fastchess_status=0
  local failure_matches

  # shellcheck disable=SC1090
  source "$job"
  mapfile -t engine_args < <(tournament_engine_args)

  CURRENT_PGN="$phase_root/games/${SHARD_ID}.${WORKER_ID}.tmp.pgn"
  final_pgn="$phase_root/games/${SHARD_ID}.pgn"
  shard_log="$phase_root/tmp/${SHARD_ID}.${WORKER_ID}.log"
  expected_games=$((PAIR_COUNT * PAIR_GAMES))
  rm -f -- "$CURRENT_PGN" "$shard_log"

  log "start shard=$SHARD_ID phase=$PHASE tc=$TC_VALUE pair_games=$PAIR_GAMES seed=$SEED"
  if (
    cd "$PIN_ROOT"
    "$FASTCHESS" \
      "${engine_args[@]}" \
      -each proto=uci "tc=$TC_VALUE" "timemargin=$TIMEMARGIN" option.Threads=1 \
      -openings "file=$BOOK" format=epd order=random plies=16 \
      -srand "$SEED" \
      -games 2 -rounds "$ROUNDS" -repeat \
      -concurrency "$CONCURRENCY" \
      -recover \
      -pgnout "file=$CURRENT_PGN" min=true \
      -scoreinterval "$expected_games" \
      -ratinginterval "$RATING_INTERVAL"
  ) > "$shard_log" 2>&1; then
    fastchess_status=0
  else
    fastchess_status=$?
  fi
  cat "$shard_log" >> "$LOG"

  if (( fastchess_status != 0 )); then
    log "retry shard=$SHARD_ID reason=fastchess_status_$fastchess_status"
    return_claim
    CURRENT_JOB=""
    CURRENT_QUEUE=""
    CURRENT_PGN=""
    rm -f -- "$shard_log"
    return 0
  fi

  failure_matches="$(grep -Ei 'illegal' "$shard_log" || true)"
  if [[ -n "$failure_matches" ]]; then
    {
      printf 'worker=%s\nshard=%s\nreason=fastchess reported a game failure\n' \
        "$WORKER_ID" "$SHARD_ID"
      printf '%s\n' "$failure_matches"
    } > "$WORK_ROOT/failed/$WORKER_ID.failed"
    return 1
  fi

  actual_games="$(grep -c '^\[Event ' "$CURRENT_PGN" || true)"
  if [[ "$actual_games" != "$expected_games" ]]; then
    log "retry shard=$SHARD_ID reason=expected_${expected_games}_games_found_$actual_games"
    return_claim
    CURRENT_JOB=""
    CURRENT_QUEUE=""
    CURRENT_PGN=""
    rm -f -- "$shard_log"
    return 0
  fi

  mv "$CURRENT_PGN" "$final_pgn"
  rm -f -- "$shard_log"
  CURRENT_PGN=""
  rm -f -- "$CURRENT_JOB"
  CURRENT_JOB=""
  CURRENT_QUEUE=""
  printf 'worker=%s\nfinished=%s\n' "$WORKER_ID" "$(date '+%Y-%m-%d %H:%M:%S')" \
    > "$phase_root/done/$SHARD_ID.done"
  log "finished shard=$SHARD_ID"
}

run_phase() {
  local phase="$1"
  local phase_root="$WORK_ROOT/$phase"
  local job

  while [[ ! -f "$phase_root/start" ]]; do
    [[ -f "$WORK_ROOT/state/run.complete" ]] && return 0
    sleep "$POLL_SECONDS"
  done

  log "$phase available"
  while [[ ! -f "$phase_root/complete" ]]; do
    if claim_job "$phase_root"; then
      run_shard "$phase_root" "$CURRENT_JOB"
    else
      sleep "$POLL_SECONDS"
    fi
  done
  log "$phase complete"
}

run_phase stc
run_phase ltc

touch "$WORK_ROOT/state/$WORKER_ID.finished"
trap - EXIT INT TERM
log "done"
