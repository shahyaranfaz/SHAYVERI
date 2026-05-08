#!/usr/bin/env bash
set -euo pipefail

# SHAYVERI NNUE feedback loop:
#   datagen -> convert each worker plain -> shuffle -> delete intermediates -> train -> delete chunk data
#
# Single chunk:
#   ./nnue_feedback_loop.sh test000
#
# Multi-chunk low-disk loop:
#   ITERS=20 ./nnue_feedback_loop.sh
#
# Common overrides:
#   POSITIONS=25000000 THREADS=23 ./nnue_feedback_loop.sh
#   ENGINE=./SHAYVERI MARLINFLOW_UTILS=~/marlinflow/target/release/marlinflow-utils ./nnue_feedback_loop.sh
#   START_ITER=5 ITERS=15 RESUME=~/marlinflow/trainer/nn/shayveri_v2_iter004_5 ./nnue_feedback_loop.sh

POSITIONS="${POSITIONS:-25000000}"
THREADS="${THREADS:-23}"
ENGINE="${ENGINE:-./SHAYVERI}"
MARLINFLOW_UTILS="${MARLINFLOW_UTILS:-$HOME/marlinflow/target/release/marlinflow-utils}"
TRAINER_DIR="${TRAINER_DIR:-$HOME/marlinflow/trainer}"
PYTHON="${PYTHON:-python3}"

RAW_DIR="${RAW_DIR:-raw}"
DATA_DIR="${DATA_DIR:-$TRAINER_DIR/data}"

TRAIN_ID="${TRAIN_ID:-shayveri_v2}"
ITERS="${ITERS:-1}"
START_ITER="${START_ITER:-0}"
RESUME="${RESUME:-}"
LR="${LR:-0.001}"
EPOCHS="${EPOCHS:-5}"
LR_DROP="${LR_DROP:-30}"
BATCH_SIZE="${BATCH_SIZE:-16384}"
WDL="${WDL:-0.3}"
SCALE="${SCALE:-400}"
SAVE_EPOCHS="${SAVE_EPOCHS:-5}"

mkdir -p "$RAW_DIR" "$DATA_DIR" "$TRAINER_DIR/nn" "$TRAINER_DIR/runs"

run_chunk() {
  local chunk_id="$1"
  local resume_path="$2"
  local prefix="${PREFIX:-chunk_${chunk_id}}"
  local run_train_id="${RUN_TRAIN_ID:-${TRAIN_ID}_${chunk_id}}"
  local plain_prefix="$RAW_DIR/$prefix"

  echo "== chunk $chunk_id =="
  echo "== datagen =="
  echo "engine=$ENGINE threads=$THREADS positions=$POSITIONS prefix=$plain_prefix"
  "$ENGINE" datagen "$THREADS" "$POSITIONS" "$plain_prefix"

  echo "== convert and shuffle worker files =="
  shopt -s nullglob
  local plain_files=("${plain_prefix}"_*.plain)
  if (( ${#plain_files[@]} == 0 )); then
    echo "no plain files found for prefix $plain_prefix" >&2
    exit 1
  fi

  for plain in "${plain_files[@]}"; do
    local stem
    local bin
    local shuffled
    stem="$(basename "$plain" .plain)"
    bin="$DATA_DIR/${stem}.bin"
    shuffled="$DATA_DIR/${stem}_shuffled.bin"

    echo "-- $plain"
    wc -l "$plain"
    "$MARLINFLOW_UTILS" txt-to-data "$plain" --output "$bin"
    rm -f "$plain"
    "$MARLINFLOW_UTILS" shuffle "$bin" --output "$shuffled"
    rm -f "$bin"
  done

  local shuffled_files=("$DATA_DIR/${prefix}"_*_shuffled.bin)

  echo "== train =="
  cd "$TRAINER_DIR"
  local train_args=(
    main.py
    --data-root "$DATA_DIR"
    --train-id "$run_train_id"
    --lr "$LR"
    --epochs "$EPOCHS"
    --lr-drop "$LR_DROP"
    --batch-size "$BATCH_SIZE"
    --wdl "$WDL"
    --scale "$SCALE"
    --save-epochs "$SAVE_EPOCHS"
  )
  if [[ -n "$resume_path" ]]; then
    train_args+=(--resume "$resume_path")
  fi
  "$PYTHON" "${train_args[@]}"
  cd - >/dev/null

  echo "== cleanup shuffled chunk data =="
  rm -f "${shuffled_files[@]}"

  local checkpoint="$TRAINER_DIR/nn/${run_train_id}_${EPOCHS}"
  if [[ ! -f "$checkpoint" ]]; then
    echo "expected checkpoint missing: $checkpoint" >&2
    exit 1
  fi
  echo "done: checkpoint=$checkpoint"
}

if [[ $# -gt 0 ]]; then
  run_chunk "$1" "$RESUME"
else
  previous_resume="$RESUME"
  for ((i = START_ITER; i < START_ITER + ITERS; ++i)); do
    chunk_id="$(printf 'iter%03d' "$i")"
    run_chunk "$chunk_id" "$previous_resume"
    previous_resume="$TRAINER_DIR/nn/${TRAIN_ID}_${chunk_id}_${EPOCHS}"
  done
fi
