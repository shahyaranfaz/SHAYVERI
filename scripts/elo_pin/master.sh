#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

RESUME_RUN_ID="${RESUME_RUN_ID:-}"
RECLAIM_WORKING="${RECLAIM_WORKING:-0}"
RESUMING=0

if [[ -n "$RESUME_RUN_ID" ]]; then
  SAFE_RUN_ID="${RESUME_RUN_ID//[^A-Za-z0-9_.-]/_}"
  [[ "$RESUME_RUN_ID" == "$SAFE_RUN_ID" ]] || \
    die "RESUME_RUN_ID may contain only letters, numbers, dots, underscores, and hyphens"
  RUN_ID="$RESUME_RUN_ID"
  WORK_ROOT="$(run_work_dir "$RUN_ID")"
  [[ -f "$WORK_ROOT/config.env" ]] || die "missing run configuration: $WORK_ROOT/config.env"
  # shellcheck disable=SC1090
  source "$WORK_ROOT/config.env"
  RESUMING=1
else
  [[ -n "$RELEASE_ID" ]] || die "RELEASE_ID is required"
  (( EXPECTED_WORKERS > 0 )) || die "EXPECTED_WORKERS must be positive"
  [[ "$RELEASE_ID" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]] || \
    die "RELEASE_ID must use the full vX.Y.Z form"
  SAFE_RELEASE_ID="${RELEASE_ID//[^A-Za-z0-9_.-]/_}"
  [[ "$RELEASE_ID" == "$SAFE_RELEASE_ID" ]] || \
    die "RELEASE_ID may contain only letters, numbers, dots, underscores, and hyphens"
  RUN_ID="${SAFE_RELEASE_ID}_$(date +%Y%m%d_%H%M%S)"
  WORK_ROOT="$(run_work_dir "$RUN_ID")"
fi

FINAL_ROOT="$OUTPUT_ROOT/$RELEASE_ID"
PUBLISH_ROOT="$WORK_ROOT/publish"

require_file "$BOOK"
require_file "$ANCHORS"
require_exe "$FASTCHESS"
[[ -x "$ORDO" ]] || die "missing executable: $ORDO"

if (( ! RESUMING )) && [[ -e "$FINAL_ROOT" && "$OVERWRITE" != "1" ]]; then
  die "output already exists: $FINAL_ROOT; set OVERWRITE=1 to replace it"
fi

mkdir -p "$WORK_ROOT"/{workers,state,failed,logs} "$PUBLISH_ROOT"
mkdir -p "$PIN_ROOT" "$WORK_ROOT_BASE" "$OUTPUT_ROOT"

LOG="$WORK_ROOT/logs/master.log"
touch "$LOG"

log() {
  printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" | tee -a "$LOG"
}

write_env() {
  local key="$1"
  local value="$2"
  printf '%s=%q\n' "$key" "$value" >> "$WORK_ROOT/config.env"
}

RUN_POINTER_TMP="$PIN_ROOT/current_run_id.tmp.$$"
printf '%s\n' "$RUN_ID" > "$RUN_POINTER_TMP"
mv "$RUN_POINTER_TMP" "$PIN_ROOT/current_run_id"

if (( RESUMING )); then
  [[ -f "$WORK_ROOT/state/registered_workers.txt" ]] || \
    die "missing frozen worker roster for $RUN_ID"
  mapfile -t workers < "$WORK_ROOT/state/registered_workers.txt"
  (( ${#workers[@]} > 0 )) || die "frozen worker roster is empty"
  log "resuming run_id=$RUN_ID release=$RELEASE_ID frozen_roster=${workers[*]}"
else
  : > "$WORK_ROOT/config.env"
  for key in PIN_ROOT SCRIPT_DIR RUN_ID WORK_ROOT ENGINES_DIR BOOK ANCHORS ORDO \
    FASTCHESS OUTPUT_ROOT RELEASE_ID REGISTER_SECONDS EXPECTED_WORKERS TIMEMARGIN CONCURRENCY \
    RATING_INTERVAL POLL_SECONDS WORKER_ACK_SECONDS BASE_SEED STC_GAMES_PER_PAIR LTC_GAMES_PER_PAIR \
    STC_SHARD_PAIR_GAMES LTC_SHARD_PAIR_GAMES NAME_ID HCE_NAME NNUE_NAME NET \
    SHAYVERI_OPTIONS FIXED_OPPONENTS ORDO_FLAGS; do
    write_env "$key" "${!key}"
  done

  log "run_id=$RUN_ID release=$RELEASE_ID"
  log "registration window=${REGISTER_SECONDS}s expected_workers=$EXPECTED_WORKERS"
  log "stc_games_per_pair=$STC_GAMES_PER_PAIR ltc_games_per_pair=$LTC_GAMES_PER_PAIR"

  deadline=$((SECONDS + REGISTER_SECONDS))
  while (( SECONDS < deadline )); do
    count="$(find "$WORK_ROOT/workers" -maxdepth 1 -type f -name '*.worker' | wc -l)"
    remaining=$((deadline - SECONDS))
    log "registered_workers=$count remaining=${remaining}s"
    (( count >= EXPECTED_WORKERS )) && break
    (( remaining > 30 )) && remaining=30
    sleep "$remaining"
  done

  mapfile -t workers < <(
    find "$WORK_ROOT/workers" -maxdepth 1 -type f -name '*.worker' -printf '%f\n' \
      | sed 's/\.worker$//' \
      | sort
  )

  (( ${#workers[@]} > 0 )) || die "no workers registered"
  printf '%s\n' "${workers[@]}" > "$WORK_ROOT/state/registered_workers.txt"
  touch "$WORK_ROOT/state/registration.closed"
  log "registration closed frozen_roster=${workers[*]}"
fi

review_failure_markers() {
  local marker

  for marker in "$WORK_ROOT/failed"/*.failed; do
    [[ -e "$marker" ]] || return 0
    if grep -Eiq 'illegal' "$marker"; then
      printf '%s\n' "$marker"
      cat "$marker"
      die "illegal-move failure remains in resumed run"
    fi
    log "discarding obsolete transient failure marker=$(basename "$marker")"
    rm -f -- "$marker"
  done
}

(( RESUMING )) && review_failure_markers

reclaim_working_jobs() {
  local phase
  local phase_root
  local job
  local working_count=0

  for phase in stc ltc; do
    phase_root="$WORK_ROOT/$phase"
    [[ -d "$phase_root/working" ]] || continue
    working_count=$((working_count + $(find "$phase_root/working" -maxdepth 1 -type f -name '*.job' | wc -l)))
  done

  (( working_count == 0 )) && return 0
  if [[ "$RECLAIM_WORKING" != "1" ]]; then
    log "preserving existing working shards=$working_count"
    return 0
  fi

  for phase in stc ltc; do
    phase_root="$WORK_ROOT/$phase"
    [[ -d "$phase_root/working" ]] || continue
    for job in "$phase_root/working"/*.job; do
      [[ -e "$job" ]] || continue
      unset SHARD_ID
      # shellcheck disable=SC1090
      source "$job"
      [[ -n "${SHARD_ID:-}" ]] || die "working job has no SHARD_ID: $job"
      mv "$job" "$phase_root/queue/$SHARD_ID.job"
      rm -f -- "$phase_root/games/$SHARD_ID".*.tmp.pgn
    done
  done
  log "requeued abandoned working shards=$working_count"
}

(( RESUMING )) && reclaim_working_jobs

create_shards() {
  local phase="$1"
  local phase_root="$WORK_ROOT/$phase"
  local total_pair_games
  local shard_pair_games
  local remaining
  local shard_index=0

  total_pair_games="$(phase_games_per_pair "$phase")"
  shard_pair_games="$(phase_shard_pair_games "$phase")"
  (( total_pair_games > 0 && total_pair_games % 2 == 0 )) || \
    die "$phase games per pair must be positive and even"
  (( shard_pair_games > 0 && shard_pair_games % 2 == 0 )) || \
    die "$phase shard games per pair must be positive and even"

  mkdir -p "$phase_root"/{queue,working,done,games,tmp}
  remaining="$total_pair_games"

  while (( remaining > 0 )); do
    local pair_games="$shard_pair_games"
    local shard_id
    local seed
    (( pair_games <= remaining )) || pair_games="$remaining"
    shard_index=$((shard_index + 1))
    printf -v shard_id '%s_%04d' "$phase" "$shard_index"
    local phase_seed_offset=0
    [[ "$phase" == "stc" ]] || phase_seed_offset=100000
    seed=$((BASE_SEED + phase_seed_offset + shard_index))

    {
      printf 'SHARD_ID=%q\n' "$shard_id"
      printf 'PHASE=%q\n' "$phase"
      printf 'TC_VALUE=%q\n' "$(phase_tc "$phase")"
      printf 'PAIR_GAMES=%q\n' "$pair_games"
      printf 'ROUNDS=%q\n' "$((pair_games / 2))"
      printf 'SEED=%q\n' "$seed"
    } > "$phase_root/queue/$shard_id.job"
    remaining=$((remaining - pair_games))
  done

  printf '%s\n' "$shard_index" > "$phase_root/shard_count"
  printf '%s\n' "$((PAIR_COUNT * total_pair_games))" > "$phase_root/expected_games"
}

count_files() {
  local dir="$1"
  local glob="$2"
  find "$dir" -maxdepth 1 -type f -name "$glob" 2>/dev/null | wc -l
}

run_ordo() {
  local phase="$1"
  local phase_root="$WORK_ROOT/$phase"
  local publish="$PUBLISH_ROOT/$phase"

  mkdir -p "$publish"
  find "$phase_root/games" -maxdepth 1 -type f -name '*.pgn' -print0 \
    | sort -z \
    | xargs -0 cat > "$publish/rating_pool.pgn"

  local expected_games
  local actual_games
  expected_games="$(< "$phase_root/expected_games")"
  actual_games="$(grep -c '^\[Event ' "$publish/rating_pool.pgn" || true)"
  [[ "$actual_games" == "$expected_games" ]] || \
    die "$phase PGN has $actual_games games, expected $expected_games"

  cp "$ANCHORS" "$phase_root/tmp/anchors"
  (
    cd "$phase_root/tmp"
    # shellcheck disable=SC2086
    "$ORDO" -p "$publish/rating_pool.pgn" $ORDO_FLAGS \
      -o results.txt -c results.csv
  ) | tee "$publish/ordo.stdout.txt"

  for output in results.txt h2h.txt results.csv cfs.csv err.csv; do
    [[ -f "$phase_root/tmp/$output" ]] || die "Ordo did not create $output for $phase"
    mv "$phase_root/tmp/$output" "$publish/$output"
  done
}

run_phase() {
  local phase="$1"
  local phase_root="$WORK_ROOT/$phase"
  local shard_count

  if [[ -f "$phase_root/complete" ]]; then
    log "$phase already complete"
    return 0
  fi

  if [[ -f "$phase_root/shard_count" ]]; then
    log "$phase resuming existing shards"
  else
    create_shards "$phase"
  fi
  shard_count="$(< "$phase_root/shard_count")"
  touch "$phase_root/start"
  log "$phase started shards=$shard_count tc=$(phase_tc "$phase")"

  while true; do
    if (( $(count_files "$WORK_ROOT/failed" '*.failed') > 0 )); then
      find "$WORK_ROOT/failed" -maxdepth 1 -type f -name '*.failed' -print -exec cat {} \;
      die "$phase worker failure"
    fi

    local done_count
    done_count="$(count_files "$phase_root/done" '*.done')"
    log "$phase progress=$done_count/$shard_count queue=$(count_files "$phase_root/queue" '*.job') working=$(count_files "$phase_root/working" '*.job')"
    (( done_count == shard_count )) && break
    sleep "$POLL_SECONDS"
  done

  run_ordo "$phase"
  touch "$phase_root/complete"
  log "$phase complete"
}

run_phase stc
run_phase ltc

touch "$WORK_ROOT/state/run.complete"
log "waiting for workers to acknowledge completion"
ack_deadline=$((SECONDS + WORKER_ACK_SECONDS))
while (( SECONDS < ack_deadline )); do
  ack_count="$(count_files "$WORK_ROOT/state" '*.finished')"
  (( ack_count == ${#workers[@]} )) && break
  sleep "$POLL_SECONDS"
done
ack_count="$(count_files "$WORK_ROOT/state" '*.finished')"
if (( ack_count != ${#workers[@]} )); then
  log "warning: worker acknowledgements=$ack_count/${#workers[@]}; continuing cleanup"
fi

if [[ -e "$FINAL_ROOT" ]]; then
  [[ "$OVERWRITE" == "1" ]] || die "refusing to replace $FINAL_ROOT"
  rm -rf -- "$FINAL_ROOT"
fi
mv "$PUBLISH_ROOT" "$FINAL_ROOT"
rm -f "$PIN_ROOT/current_run_id"

case "$WORK_ROOT" in
  "$WORK_ROOT_BASE"/*) rm -rf -- "$WORK_ROOT" ;;
  *) die "refusing to clean unexpected work directory: $WORK_ROOT" ;;
esac

printf 'release pin complete: %s\n' "$FINAL_ROOT"
