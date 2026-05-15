#!/usr/bin/env bash
set -euo pipefail

# Worker-side producer for SHAYVERI net13.
# Generates the exact branch lanes described in net13results.txt:
# baseline chunks use regular 10k-node self-play;
# deep-hard chunks generate shallow self-play, then relabel/filter with deeper
# net5 search through deep_relabel.py.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/branch_config.sh"

POSITIONS="${POSITIONS:-1000000}"
THREADS="${THREADS:-23}"
BASELINE_NODES="${BASELINE_NODES:-10000}"
SHALLOW_NODES="${SHALLOW_NODES:-10000}"
DEEP_NODES="${DEEP_NODES:-100000}"
DEEP_RAW_MULTIPLIER="${DEEP_RAW_MULTIPLIER:-3}"
SLEEP_SECONDS="${SLEEP_SECONDS:-5}"
MIN_FREE_KB="${MIN_FREE_KB:-2000000}"
WORKER_ID="${WORKER_ID:-$(hostname)}"
START_CHUNK="${START_CHUNK:-0}"
CHUNKS="${CHUNKS:-20}" # 0 means run forever.
PYTHON="${PYTHON:-python3}"

RUN_ROOT="${RUN_ROOT:-$PWD}"
NET5_DIR="${NET5_DIR:-$RUN_ROOT}"
HCE_DIR="${HCE_DIR:-$NFS_ROOT/no_net_dir}"
NET5_ENGINE="${NET5_ENGINE:-./SHAYVERI}"
HCE_ENGINE="${HCE_ENGINE:-$RUN_ROOT/SHAYVERI}"
NET5_EVAL_FILE="${NET5_EVAL_FILE:-net5_final.nnue}"
RELABEL_ENGINE="${RELABEL_ENGINE:-$NET5_DIR/SHAYVERI}"
RELABEL_EVAL_FILE="${RELABEL_EVAL_FILE:-$NET5_DIR/$NET5_EVAL_FILE}"
RELABEL_THREADS="${RELABEL_THREADS:-1}"
RELABEL_HASH="${RELABEL_HASH:-64}"
HARD_CP_DIFF="${HARD_CP_DIFF:-80}"
HARD_NEAR_EQ="${HARD_NEAR_EQ:-150}"
HARD_DRAW_MARGIN="${HARD_DRAW_MARGIN:-150}"

INCOMING_DIR="$NFS_ROOT/incoming"
READY_DIR="$NFS_ROOT/ready"
FAILED_DIR="$NFS_ROOT/failed"

mkdir -p "$INCOMING_DIR" "$READY_DIR" "$FAILED_DIR"
mkdir -p "$HCE_DIR"

if [[ ! -x "$NET5_DIR/SHAYVERI" ]]; then
  echo "missing executable net5 SHAYVERI: $NET5_DIR/SHAYVERI" >&2
  exit 1
fi
if [[ ! -f "$NET5_DIR/$NET5_EVAL_FILE" ]]; then
  echo "missing net5 eval file: $NET5_DIR/$NET5_EVAL_FILE" >&2
  exit 1
fi
if [[ ! -x "$HCE_ENGINE" ]]; then
  echo "missing executable HCE engine command: $HCE_ENGINE" >&2
  exit 1
fi
if [[ ! -x "$RELABEL_ENGINE" ]]; then
  echo "missing executable relabel engine: $RELABEL_ENGINE" >&2
  exit 1
fi
if [[ ! -f "$RELABEL_EVAL_FILE" ]]; then
  echo "missing relabel eval file: $RELABEL_EVAL_FILE" >&2
  exit 1
fi

check_free_space() {
  local avail_kb
  avail_kb="$(df -Pk "$NFS_ROOT" | awk 'NR==2 {print $4}')"
  if (( avail_kb < MIN_FREE_KB )); then
    echo "not enough free disk under $NFS_ROOT: ${avail_kb}KB available, need ${MIN_FREE_KB}KB" >&2
    exit 1
  fi
}

finish_failed() {
  local dir="${CURRENT_CHUNK_DIR:-}"
  if [[ -n "$dir" && -d "$dir" ]]; then
    local base
    base="$(basename "$dir")"
    mv "$dir" "$FAILED_DIR/${base}.failed.$(date +%s)" 2>/dev/null || true
  fi
}
trap finish_failed ERR INT TERM

token_metadata() {
  local token="$1"
  case "$token" in
    N|n) echo "source=net5 lane=baseline generator_dir=$NET5_DIR generator_engine=$NET5_ENGINE token=N" ;;
    H|h) echo "source=hce lane=baseline generator_dir=$HCE_DIR generator_engine=$HCE_ENGINE token=H" ;;
    D|d) echo "source=net5 lane=deep-hard generator_dir=$NET5_DIR generator_engine=$NET5_ENGINE token=D" ;;
    E|e) echo "source=hce lane=deep-hard generator_dir=$HCE_DIR generator_engine=$HCE_ENGINE token=E" ;;
    *) echo "bad NET13_PATTERN token '$token' in $NET13_PATTERN" >&2; exit 1 ;;
  esac
}

generator_for_token() {
  local token="$1"
  case "$token" in
    N|n|D|d) echo "$NET5_DIR"$'\t'"$NET5_ENGINE" ;;
    H|h|E|e) echo "$HCE_DIR"$'\t'"$HCE_ENGINE" ;;
    *) echo "bad NET13_PATTERN token '$token' in $NET13_PATTERN" >&2; exit 1 ;;
  esac
}

is_deep_token() {
  local token="$1"
  [[ "$token" == "D" || "$token" == "d" || "$token" == "E" || "$token" == "e" ]]
}

run_datagen() {
  local token="$1"
  local positions="$2"
  local prefix="$3"
  local nodes="$4"
  local gen_dir
  local gen_engine
  IFS=$'\t' read -r gen_dir gen_engine < <(generator_for_token "$token")
  (
    cd "$gen_dir"
    "$gen_engine" datagen "$THREADS" "$positions" "$prefix" "$nodes"
  )
}

chunk_index="$START_CHUNK"
made=0

while (( CHUNKS == 0 || made < CHUNKS )); do
  for ((i = 0; i < ${#NET13_PATTERN}; ++i)); do
    (( CHUNKS > 0 && made >= CHUNKS )) && break

    token="${NET13_PATTERN:$i:1}"
    metadata="$(token_metadata "$token")"
    source_name="$(awk '{for (i=1; i<=NF; ++i) if ($i ~ /^source=/) {sub(/^source=/, "", $i); print $i}}' <<< "$metadata")"
    lane_name="$(awk '{for (i=1; i<=NF; ++i) if ($i ~ /^lane=/) {sub(/^lane=/, "", $i); print $i}}' <<< "$metadata")"
    chunk_id="$(printf '%s_%s_%s_%06d_%s' "$lane_name" "$source_name" "$WORKER_ID" "$chunk_index" "$(date +%s)")"
    CURRENT_CHUNK_DIR="$INCOMING_DIR/$chunk_id"
    ready_path="$READY_DIR/$chunk_id"
    prefix="$CURRENT_CHUNK_DIR/chunk"

    check_free_space
    mkdir "$CURRENT_CHUNK_DIR"
    {
      echo "worker=$WORKER_ID"
      echo "branch=${BRANCH:-13A}"
      echo "pattern=$NET13_PATTERN"
      for kv in $metadata; do echo "$kv"; done
      echo "chunk_index=$chunk_index"
      echo "positions=$POSITIONS"
      echo "threads=$THREADS"
      echo "baseline_nodes=$BASELINE_NODES"
      echo "shallow_nodes=$SHALLOW_NODES"
      echo "deep_nodes=$DEEP_NODES"
      echo "started_at=$(date -Is)"
    } > "$CURRENT_CHUNK_DIR/metadata.txt"

    echo "== net13 chunk $chunk_id =="
    if is_deep_token "$token"; then
      raw_prefix="$CURRENT_CHUNK_DIR/raw"
      raw_positions=$((POSITIONS * DEEP_RAW_MULTIPLIER))
      echo "deep-hard raw datagen token=$token positions=$raw_positions nodes=$SHALLOW_NODES"
      run_datagen "$token" "$raw_positions" "$raw_prefix" "$SHALLOW_NODES" \
        > "$CURRENT_CHUNK_DIR/datagen.stdout.log" \
        2> "$CURRENT_CHUNK_DIR/datagen.stderr.log"

      echo "deep-hard relabel target=$POSITIONS deep_nodes=$DEEP_NODES"
      "$PYTHON" "$SCRIPT_DIR/deep_relabel.py" \
        --engine "$RELABEL_ENGINE" \
        --eval-file "$RELABEL_EVAL_FILE" \
        --input-glob "$CURRENT_CHUNK_DIR/raw_*.plain" \
        --output "$CURRENT_CHUNK_DIR/chunk_deep.plain" \
        --shallow-nodes "$SHALLOW_NODES" \
        --deep-nodes "$DEEP_NODES" \
        --cp-diff "$HARD_CP_DIFF" \
        --near-eq "$HARD_NEAR_EQ" \
        --draw-margin "$HARD_DRAW_MARGIN" \
        --threads "$RELABEL_THREADS" \
        --hash "$RELABEL_HASH" \
        --target-positions "$POSITIONS" \
        > "$CURRENT_CHUNK_DIR/relabel.stdout.log" \
        2> "$CURRENT_CHUNK_DIR/relabel.stderr.log"
      rm -f "$CURRENT_CHUNK_DIR"/raw_*.plain
    else
      echo "baseline datagen token=$token positions=$POSITIONS nodes=$BASELINE_NODES"
      run_datagen "$token" "$POSITIONS" "$prefix" "$BASELINE_NODES" \
        > "$CURRENT_CHUNK_DIR/datagen.stdout.log" \
        2> "$CURRENT_CHUNK_DIR/datagen.stderr.log"
    fi

    check_free_space
    echo "finished_at=$(date -Is)" >> "$CURRENT_CHUNK_DIR/metadata.txt"
    touch "$CURRENT_CHUNK_DIR/DONE"
    mv "$CURRENT_CHUNK_DIR" "$ready_path"
    CURRENT_CHUNK_DIR=""

    echo "published $ready_path"
    chunk_index=$((chunk_index + 1))
    made=$((made + 1))
    sleep "$SLEEP_SECONDS"
  done
done
