#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="${SCRIPT_DIR:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)}"

RUN_ROOT="${RUN_ROOT:-$HOME/shayveri_v3_datagen}"
INCOMPLETE_ROOT="${INCOMPLETE_ROOT:-$RUN_ROOT/incomplete}"
READY_ROOT="${READY_ROOT:-$RUN_ROOT/ready}"
STATE_DIR="${STATE_DIR:-$RUN_ROOT/state}"
LOG_DIR="${LOG_DIR:-$RUN_ROOT/logs}"

ENGINE_DIR="${ENGINE_DIR:-$HOME/chess_arena/chess_bot}"
ENGINE="${ENGINE:-./SHAYVERI}"
EVAL_FILE="${EVAL_FILE:-SHAYVERI2_5_0.nnue}"

THREADS="${THREADS:-23}"
SHARD_POSITIONS="${SHARD_POSITIONS:-${POSITIONS_PER_SHARD:-5000000}}"
NODES="${NODES:-2500}"
SAMPLE_STRIDE="${SAMPLE_STRIDE:-1}"
MAX_SAMPLES_PER_GAME="${MAX_SAMPLES_PER_GAME:-16}"
ENABLE_ADJUDICATION="${ENABLE_ADJUDICATION:-false}"
ADJUDICATION_CP="${ADJUDICATION_CP:-3000}"
ADJUDICATION_PLIES="${ADJUDICATION_PLIES:-8}"
START_FILE="${START_FILE:-}"
START_FILE_PROB="${START_FILE_PROB:-0}"
START_SHARD="${START_SHARD:-0}"
SHARDS="${SHARDS:-0}" # 0 means run forever.
SEED_BASE="${SEED_BASE:-300000}"
PRINT_INTERVAL="${PRINT_INTERVAL:-1000}"
WORKER_ID="${WORKER_ID:-$(hostname 2>/dev/null || echo worker)}"
ADJUDICATION_LABEL="adj"
if [[ "$ENABLE_ADJUDICATION" == "false" || "$ENABLE_ADJUDICATION" == "0" || "$ENABLE_ADJUDICATION" == "no" ]]; then
  ADJUDICATION_LABEL="noadj"
fi
SHARD_PREFIX="${SHARD_PREFIX:-${WORKER_ID}_n${NODES}_s${MAX_SAMPLES_PER_GAME}_${ADJUDICATION_LABEL}}"

SLEEP_SECONDS="${SLEEP_SECONDS:-2}"
BACKPRESSURE_SLEEP_SECONDS="${BACKPRESSURE_SLEEP_SECONDS:-30}"
MAX_UNTRANSFERRED_SHARDS="${MAX_UNTRANSFERRED_SHARDS:-12}"
MIN_FREE_KB="${MIN_FREE_KB:-1000000}"

BRIDGE_DEST="${BRIDGE_DEST:-shahy@localhost:~/shayveri_v3_datagen/bullet}"
RSYNC="${RSYNC:-rsync}"
RSYNC_SSH="${RSYNC_SSH:-ssh -i ~/.ssh/net15_wsl_bridge -p 2222}"
RSYNC_ARGS="${RSYNC_ARGS:--a --partial --inplace}"

die() {
  echo "error: $*" >&2
  exit 1
}

safe_id() {
  local value="$1"
  value="${value//[^A-Za-z0-9._-]/_}"
  printf '%s' "$value"
}

require_engine() {
  [[ -d "$ENGINE_DIR" ]] || die "missing ENGINE_DIR: $ENGINE_DIR"
  if [[ "$ENGINE" == /* ]]; then
    [[ -x "$ENGINE" ]] || die "missing engine: $ENGINE"
  else
    [[ -x "$ENGINE_DIR/$ENGINE" ]] || die "missing engine: $ENGINE_DIR/$ENGINE"
  fi
  if [[ "$EVAL_FILE" != "<hce>" && "$EVAL_FILE" != /* ]]; then
    [[ -f "$ENGINE_DIR/$EVAL_FILE" ]] || die "missing EVAL_FILE: $ENGINE_DIR/$EVAL_FILE"
  fi
}

init_dirs() {
  mkdir -p "$INCOMPLETE_ROOT" "$READY_ROOT" "$STATE_DIR" "$LOG_DIR"
}

ready_count() {
  find "$READY_ROOT" -mindepth 1 -maxdepth 1 -type d ! -name "*.transfer" 2>/dev/null | wc -l
}

transfer_count() {
  find "$READY_ROOT" -mindepth 1 -maxdepth 1 -type d -name "*.transfer" 2>/dev/null | wc -l
}

incomplete_count() {
  find "$INCOMPLETE_ROOT" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l
}

dir_bullet_positions() {
  local dir="$1"
  find "$dir" -type f -name '*.bullet.bin' -printf '%s\n' 2>/dev/null \
    | awk '{s += $1} END {printf "%d", s / 32}'
}

write_env_line() {
  local key="$1"
  local value="$2"
  printf '%s=%q\n' "$key" "$value"
}
