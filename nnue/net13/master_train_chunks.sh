#!/usr/bin/env bash
set -euo pipefail

# Master-side consumer for SHAYVERI net13.
# Claims chunks according to the same branch pattern as the worker, converts
# plain data into Marlinflow bins, and trains the known-good Chess768/256 path.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/branch_config.sh"

MARLINFLOW_UTILS="${MARLINFLOW_UTILS:-$HOME/marlinflow/target/release/marlinflow-utils}"
TRAINER_DIR="${TRAINER_DIR:-$HOME/marlinflow/trainer}"
PYTHON="${PYTHON:-python3}"

READY_DIR="$NFS_ROOT/ready"
CLAIMED_DIR="$NFS_ROOT/claimed"
DONE_DIR="$NFS_ROOT/done"
FAILED_DIR="$NFS_ROOT/failed"
LOCK_DIR="$NFS_ROOT/master.lock"
STATE_DIR="$NFS_ROOT/state"

DATA_DIR="${DATA_DIR:-$TRAINER_DIR/data}"
START_ITER="${START_ITER:-0}"
MAX_CHUNKS="${MAX_CHUNKS:-20}"
POLL_SECONDS="${POLL_SECONDS:-10}"
RESUME="${RESUME:-$HOME/marlinflow/trainer/nn/shayveri_v2.4_iter064_1}"
MIN_CHUNK_LINES="${MIN_CHUNK_LINES:-900000}"

LR_DROP_CHUNK="${LR_DROP_CHUNK:-100}"
LR_AFTER_DROP="${LR_AFTER_DROP:-0.00001}"
SECOND_LR_DROP_CHUNK="${SECOND_LR_DROP_CHUNK:-200}"
SECOND_LR_AFTER_DROP="${SECOND_LR_AFTER_DROP:-0.00001}"
EPOCHS="${EPOCHS:-1}"
BATCH_SIZE="${BATCH_SIZE:-16384}"
WDL="${WDL:-0.1}"
SCALE="${SCALE:-400}"
SAVE_EPOCHS="${SAVE_EPOCHS:-1}"

mkdir -p "$READY_DIR" "$CLAIMED_DIR" "$DONE_DIR" "$FAILED_DIR" "$STATE_DIR"
mkdir -p "$DATA_DIR" "$TRAINER_DIR/nn" "$TRAINER_DIR/runs"

if [[ ! -x "$MARLINFLOW_UTILS" ]]; then
  echo "missing marlinflow-utils: $MARLINFLOW_UTILS" >&2
  exit 1
fi
if [[ ! -d "$TRAINER_DIR" ]]; then
  echo "missing TRAINER_DIR: $TRAINER_DIR" >&2
  exit 1
fi
if [[ -n "$RESUME" && ! -f "$RESUME" ]]; then
  echo "resume checkpoint not found: $RESUME" >&2
  echo "set RESUME to the net5 iter064 PyTorch checkpoint, or reconstruct it first" >&2
  exit 1
fi

if ! mkdir "$LOCK_DIR" 2>/dev/null; then
  echo "another master appears to be running: $LOCK_DIR" >&2
  exit 1
fi
trap 'rmdir "$LOCK_DIR" 2>/dev/null || true' EXIT

chunk_lr() {
  local iter="$1"
  if (( iter >= SECOND_LR_DROP_CHUNK )); then
    echo "$SECOND_LR_AFTER_DROP"
  elif (( iter >= LR_DROP_CHUNK )); then
    echo "$LR_AFTER_DROP"
  else
    echo "$LR"
  fi
}

desired_token_for_iter() {
  local iter="$1"
  local len="${#NET13_PATTERN}"
  local slot=$((iter % len))
  echo "${NET13_PATTERN:$slot:1}" | tr '[:lower:]' '[:upper:]'
}

find_ready_chunk() {
  local desired_token="$1"
  find "$READY_DIR" -mindepth 1 -maxdepth 1 -type d | sort | while read -r dir; do
    [[ -f "$dir/DONE" ]] || continue
    local token
    token="$(sed -n 's/^token=//p' "$dir/metadata.txt" 2>/dev/null | head -n 1 | tr '[:lower:]' '[:upper:]')"
    if [[ "$token" == "$desired_token" ]]; then
      echo "$dir"
      break
    fi
  done
}

claim_chunk() {
  local ready="$1"
  local base
  base="$(basename "$ready")"
  local claimed="$CLAIMED_DIR/$base"
  if mv "$ready" "$claimed" 2>/dev/null; then
    echo "$claimed"
    return 0
  fi
  return 1
}

mark_failed() {
  local chunk_dir="$1"
  local iter="$2"
  local base
  base="$(basename "$chunk_dir")"
  echo "failed_at=$(date -Is)" >> "$chunk_dir/master.log" 2>/dev/null || true
  echo "iter=$iter" >> "$chunk_dir/master.log" 2>/dev/null || true
  mv "$chunk_dir" "$FAILED_DIR/${base}.failed.$(date +%s)" 2>/dev/null || true
}

process_chunk() {
  local chunk_dir="$1"
  local iter="$2"
  local resume_path="$3"
  local iter_id
  local run_train_id
  local lr_now
  iter_id="$(printf 'iter%03d' "$iter")"
  run_train_id="${TRAIN_ID}_${iter_id}"
  lr_now="$(chunk_lr "$iter")"

  {
    echo "claimed_at=$(date -Is)"
    echo "iter=$iter"
    echo "train_id=$run_train_id"
    echo "lr=$lr_now"
    echo "resume=$resume_path"
  } >> "$chunk_dir/master.log"

  echo "== process $(basename "$chunk_dir") as $iter_id =="

  shopt -s nullglob
  local plain_files=("$chunk_dir"/*.plain)
  if (( ${#plain_files[@]} == 0 )); then
    echo "no .plain files in $chunk_dir" >&2
    return 1
  fi

  local total_lines=0
  for plain in "${plain_files[@]}"; do
    local stem
    local bin
    local shuffled
    local line_count
    stem="${run_train_id}_$(basename "$plain" .plain)"
    bin="$DATA_DIR/${stem}.bin"
    shuffled="$DATA_DIR/${stem}_shuffled.bin"

    echo "-- convert $(basename "$plain")"
    line_count="$(wc -l < "$plain")" || return 1
    total_lines=$((total_lines + line_count))
    echo "$line_count $plain"
    "$MARLINFLOW_UTILS" txt-to-data "$plain" --output "$bin" || return 1
    rm -f "$plain"
    "$MARLINFLOW_UTILS" shuffle "$bin" --output "$shuffled" || return 1
    rm -f "$bin"
  done

  if (( total_lines < MIN_CHUNK_LINES )); then
    echo "chunk $chunk_dir has only $total_lines lines; expected at least $MIN_CHUNK_LINES" >&2
    return 1
  fi
  echo "positions=$total_lines" >> "$chunk_dir/master.log"

  local shuffled_files=("$DATA_DIR/${run_train_id}"_*_shuffled.bin)
  if (( ${#shuffled_files[@]} == 0 )); then
    echo "no shuffled bins for $run_train_id" >&2
    return 1
  fi

  echo "== train $run_train_id =="
  pushd "$TRAINER_DIR" >/dev/null
  local train_args=(
    main.py
    --data-root "$DATA_DIR"
    --train-id "$run_train_id"
    --lr "$lr_now"
    --epochs "$EPOCHS"
    --batch-size "$BATCH_SIZE"
    --wdl "$WDL"
    --scale "$SCALE"
    --save-epochs "$SAVE_EPOCHS"
  )
  if [[ -n "$resume_path" ]]; then
    train_args+=(--resume "$resume_path")
  fi
  "$PYTHON" "${train_args[@]}"
  popd >/dev/null

  echo "== cleanup chunk data =="
  rm -f "${shuffled_files[@]}"

  local checkpoint="$TRAINER_DIR/nn/${run_train_id}_${EPOCHS}"
  if [[ ! -f "$checkpoint" ]]; then
    echo "expected checkpoint missing: $checkpoint" >&2
    return 1
  fi

  echo "$checkpoint" > "$STATE_DIR/latest_checkpoint.txt"
  echo "$((iter + 1))" > "$STATE_DIR/next_iter.txt"
  echo "checkpoint=$checkpoint" >> "$chunk_dir/master.log"
  echo "finished_at=$(date -Is)" >> "$chunk_dir/master.log"

  local base
  base="$(basename "$chunk_dir")"
  mv "$chunk_dir" "$DONE_DIR/${iter_id}_${base}" || return 1
  echo "done $iter_id checkpoint=$checkpoint"
}

if [[ -f "$STATE_DIR/next_iter.txt" ]]; then
  next_iter="$(cat "$STATE_DIR/next_iter.txt")"
else
  next_iter="$START_ITER"
fi

if [[ -z "$RESUME" && -f "$STATE_DIR/latest_checkpoint.txt" ]]; then
  RESUME="$(cat "$STATE_DIR/latest_checkpoint.txt")"
fi

processed=0
while (( MAX_CHUNKS == 0 || processed < MAX_CHUNKS )); do
  desired_token="$(desired_token_for_iter "$next_iter")"
  ready_chunk="$(find_ready_chunk "$desired_token" || true)"
  if [[ -z "$ready_chunk" ]]; then
    echo "no ready token=${desired_token} chunks; sleeping ${POLL_SECONDS}s"
    sleep "$POLL_SECONDS"
    continue
  fi

  if ! claimed_chunk="$(claim_chunk "$ready_chunk")"; then
    continue
  fi

  if process_chunk "$claimed_chunk" "$next_iter" "$RESUME"; then
    RESUME="$(cat "$STATE_DIR/latest_checkpoint.txt")"
    next_iter="$(cat "$STATE_DIR/next_iter.txt")"
    processed=$((processed + 1))
  else
    mark_failed "$claimed_chunk" "$next_iter"
    exit 1
  fi
done
