#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=datagen_common.sh
source "$SCRIPT_DIR/datagen_common.sh"

WORKER_ID="$(safe_id "$WORKER_ID")"
SHARD_PREFIX="$(safe_id "$SHARD_PREFIX")"

init_dirs
require_engine

case "$SHARD_PREFIX" in
  *[!A-Za-z0-9._-]*|"")
    die "bad SHARD_PREFIX: $SHARD_PREFIX"
    ;;
esac

wait_for_quota_headroom() {
  local count
  while true; do
    count="$(ready_count)"
    if (( count < MAX_UNTRANSFERRED_SHARDS )); then
      return
    fi
    echo "ready queue has $count shards; waiting below $MAX_UNTRANSFERRED_SHARDS before new datagen"
    sleep "$BACKPRESSURE_SLEEP_SECONDS"
  done
}

check_free_space() {
  local avail_kb
  avail_kb="$(df -Pk "$RUN_ROOT" | awk 'NR==2 {print $4}')"
  if (( avail_kb < MIN_FREE_KB )); then
    die "not enough free disk under $RUN_ROOT: ${avail_kb}KB available, need ${MIN_FREE_KB}KB"
  fi
}

next_available_shard() {
  local shard="$1"
  local shard_name
  while true; do
    shard_name="$(printf '%s_shard_%06d' "$SHARD_PREFIX" "$shard")"
    if [[ ! -e "$INCOMPLETE_ROOT/$shard_name" && ! -e "$READY_ROOT/$shard_name" && ! -e "$READY_ROOT/${shard_name}.transfer" ]]; then
      printf '%s' "$shard"
      return
    fi
    shard=$((shard + 1))
  done
}

run_datagen() {
  local prefix="$1"
  local seed="$2"
  local eval_arg=()
  local start_file_arg=()

  if [[ "$EVAL_FILE" == /* || "$EVAL_FILE" == "<hce>" || "$EVAL_FILE" == "<embedded>" || "$EVAL_FILE" == "<default>" ]]; then
    eval_arg=(--eval-file "$EVAL_FILE")
  else
    eval_arg=(--eval-file "$EVAL_FILE")
  fi

  if [[ -n "$START_FILE" ]]; then
    start_file_arg=(--start-file "$START_FILE" --start-file-prob "$START_FILE_PROB")
  fi

  (
    cd "$ENGINE_DIR"
    "$ENGINE" datagen \
      --output-prefix "$prefix" \
      --output-format bullet-v1 \
      "${eval_arg[@]}" \
      --positions "$SHARD_POSITIONS" \
      --nodes "$NODES" \
      --threads "$THREADS" \
      --seed "$seed" \
      "${start_file_arg[@]}" \
      --sample-stride "$SAMPLE_STRIDE" \
      --max-samples-per-game "$MAX_SAMPLES_PER_GAME" \
      --enable-adjudication "$ENABLE_ADJUDICATION" \
      --adjudication-cp "$ADJUDICATION_CP" \
      --adjudication-plies "$ADJUDICATION_PLIES" \
      --print-interval "$PRINT_INTERVAL"
  )
}

shard="$START_SHARD"
made=0

while (( SHARDS == 0 || made < SHARDS )); do
  wait_for_quota_headroom
  check_free_space

  shard="$(next_available_shard "$shard")"
  shard_name="$(printf '%s_shard_%06d' "$SHARD_PREFIX" "$shard")"
  tmp_dir="$INCOMPLETE_ROOT/$shard_name"
  ready_dir="$READY_ROOT/$shard_name"
  seed=$((SEED_BASE + shard))

  mkdir "$tmp_dir"
  {
    write_env_line shard "$shard"
    write_env_line shard_id "$shard_name"
    write_env_line source "shayveri-v3"
    write_env_line positions "$SHARD_POSITIONS"
    write_env_line threads "$THREADS"
    write_env_line nodes "$NODES"
    write_env_line sample_stride "$SAMPLE_STRIDE"
    write_env_line max_samples_per_game "$MAX_SAMPLES_PER_GAME"
    write_env_line enable_adjudication "$ENABLE_ADJUDICATION"
    write_env_line adjudication_cp "$ADJUDICATION_CP"
    write_env_line adjudication_plies "$ADJUDICATION_PLIES"
    write_env_line start_file "$START_FILE"
    write_env_line start_file_prob "$START_FILE_PROB"
    write_env_line seed "$seed"
    write_env_line worker "$WORKER_ID"
    write_env_line shard_prefix "$SHARD_PREFIX"
    write_env_line engine_dir "$ENGINE_DIR"
    write_env_line engine "$ENGINE"
    write_env_line eval_file "$EVAL_FILE"
    write_env_line started_at "$(date -Is)"
  } > "$tmp_dir/metadata.env"

  echo "== generate $shard_name positions=$SHARD_POSITIONS nodes=$NODES seed=$seed =="
  if ! run_datagen "$tmp_dir/data" "$seed" \
      > "$tmp_dir/datagen.stdout.log" \
      2> "$tmp_dir/datagen.stderr.log"; then
    echo "datagen failed; leaving incomplete shard for inspection: $tmp_dir" >&2
    exit 1
  fi

  actual_positions="$(dir_bullet_positions "$tmp_dir")"
  {
    write_env_line actual_positions "$actual_positions"
    write_env_line finished_at "$(date -Is)"
  } >> "$tmp_dir/metadata.env"

  if (( actual_positions != SHARD_POSITIONS )); then
    echo "warning: expected $SHARD_POSITIONS positions, found $actual_positions" >&2
  fi

  touch "$tmp_dir/DONE"
  mv "$tmp_dir" "$ready_dir"
  echo "$shard" > "$STATE_DIR/latest_generated_shard.txt"

  echo "ready $shard_name actual_positions=$actual_positions"
  shard=$((shard + 1))
  made=$((made + 1))
  sleep "$SLEEP_SECONDS"
done
