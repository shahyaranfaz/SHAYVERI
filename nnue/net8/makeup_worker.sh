#!/usr/bin/env bash
set -euo pipefail

# Simple makeup producer: generate exactly two HCE chunks.

NFS_ROOT="${NFS_ROOT:-$HOME/nnue_v2_7}"
ENGINE="${ENGINE:-./SHAYVERI}"
THREADS="${THREADS:-23}"
POSITIONS="${POSITIONS:-5000000}"
NODES="${NODES:-10000}"
SLEEP_SECONDS="${SLEEP_SECONDS:-5}"
WORKER_ID="${WORKER_ID:-makeup_$(hostname)}"
START_CHUNK="${START_CHUNK:-0}"
CHUNKS=2

INCOMING_DIR="$NFS_ROOT/incoming"
READY_DIR="$NFS_ROOT/ready"
FAILED_DIR="$NFS_ROOT/failed"

NO_NET_DIR="${NO_NET_DIR:-$HOME/chess_arena/chess_bot/no_net_dir}"

mkdir -p "$INCOMING_DIR" "$READY_DIR" "$FAILED_DIR"

finish_failed() {
  local dir="${CURRENT_CHUNK_DIR:-}"
  if [[ -n "$dir" && -d "$dir" ]]; then
    local base
    base="$(basename "$dir")"
    mv "$dir" "$FAILED_DIR/${base}.failed.$(date +%s)" 2>/dev/null || true
  fi
}
trap finish_failed ERR INT TERM

chunk_index="$START_CHUNK"
made=0

cd "$NO_NET_DIR"

while (( made < CHUNKS )); do
  chunk_id="$(printf 'hce_%s_%06d_%s' "$WORKER_ID" "$chunk_index" "$(date +%s)")"
  CURRENT_CHUNK_DIR="$INCOMING_DIR/$chunk_id"
  ready_path="$READY_DIR/$chunk_id"
  prefix="$CURRENT_CHUNK_DIR/chunk"

  mkdir "$CURRENT_CHUNK_DIR"
  {
    echo "worker=$WORKER_ID"
    echo "source=hce"
    echo "chunk_index=$chunk_index"
    echo "positions=$POSITIONS"
    echo "threads=$THREADS"
    echo "nodes=$NODES"
    echo "engine=$ENGINE"
    echo "started_at=$(date -Is)"
  } > "$CURRENT_CHUNK_DIR/metadata.txt"

  echo "== datagen chunk $chunk_id =="
  echo "engine=$ENGINE threads=$THREADS positions=$POSITIONS nodes=$NODES prefix=$prefix"
  "$ENGINE" datagen "$THREADS" "$POSITIONS" "$prefix" "$NODES" \
    > "$CURRENT_CHUNK_DIR/datagen.stdout.log" \
    2> "$CURRENT_CHUNK_DIR/datagen.stderr.log"

  echo "finished_at=$(date -Is)" >> "$CURRENT_CHUNK_DIR/metadata.txt"
  touch "$CURRENT_CHUNK_DIR/DONE"
  mv "$CURRENT_CHUNK_DIR" "$ready_path"
  CURRENT_CHUNK_DIR=""

  echo "published $ready_path"
  chunk_index=$((chunk_index + 1))
  made=$((made + 1))
  sleep "$SLEEP_SECONDS"
done
