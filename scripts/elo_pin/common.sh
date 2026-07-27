#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="${SCRIPT_DIR:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)}"
PIN_ROOT="${PIN_ROOT:-$SCRIPT_DIR}"
RUN_ID="${RUN_ID:-}"

ENGINES_DIR="${ENGINES_DIR:-$PIN_ROOT/engines}"
BOOK="${BOOK:-$PIN_ROOT/books/UHO_2024_8mvs.epd}"
ANCHORS="${ANCHORS:-$PIN_ROOT/anchors}"
ORDO="${ORDO:-$PIN_ROOT/ordo}"
FASTCHESS="${FASTCHESS:-fastchess}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
OUTPUT_ROOT="${OUTPUT_ROOT:-$PIN_ROOT/outputs}"
WORK_ROOT_BASE="${WORK_ROOT_BASE:-$PIN_ROOT/.work}"

RELEASE_ID="${RELEASE_ID:-}"
REGISTER_SECONDS="${REGISTER_SECONDS:-300}"
EXPECTED_WORKERS="${EXPECTED_WORKERS:-10}"
TIMEMARGIN="${TIMEMARGIN:-1000}"
CONCURRENCY="${CONCURRENCY:-23}"
RATING_INTERVAL="${RATING_INTERVAL:-0}"
POLL_SECONDS="${POLL_SECONDS:-5}"
WORKER_ACK_SECONDS="${WORKER_ACK_SECONDS:-120}"
BASE_SEED="${BASE_SEED:-270000}"
OVERWRITE="${OVERWRITE:-0}"

STC_GAMES_PER_PAIR="${STC_GAMES_PER_PAIR:-800}"
LTC_GAMES_PER_PAIR="${LTC_GAMES_PER_PAIR:-200}"
STC_SHARD_PAIR_GAMES="${STC_SHARD_PAIR_GAMES:-20}"
LTC_SHARD_PAIR_GAMES="${LTC_SHARD_PAIR_GAMES:-10}"

NAME_ID="${NAME_ID:-SHAYVERI $RELEASE_ID / NNUE}"
HCE_NAME="${HCE_NAME:-SHAYVERI $RELEASE_ID / HCE}"
NNUE_NAME="${NNUE_NAME:-$NAME_ID}"
NET="${NET:-}"
SHAYVERI_OPTIONS="${SHAYVERI_OPTIONS:-option.OwnBook=false option.BookInfoDepth=0}"

FIXED_OPPONENTS="Alexandria9,Berserk13,Ethereal14,PlentyChess7,Weiss2,SF2850,SF3000"
ENGINE_COUNT=9
PAIR_COUNT=36
ORDO_FLAGS="${ORDO_FLAGS:--m anchors -W -D -s 5000 -n 23 -J -j h2h.txt -C cfs.csv -e err.csv -F 99}"

die() {
  echo "error: $*" >&2
  exit 1
}

require_file() {
  [[ -f "$1" ]] || die "missing file: $1"
}

require_exe() {
  command -v "$1" >/dev/null 2>&1 || [[ -x "$1" ]] || die "missing executable: $1"
}

current_run_id() {
  [[ -f "$PIN_ROOT/current_run_id" ]] || die "no current run id at $PIN_ROOT/current_run_id"
  tr -d '\r\n' < "$PIN_ROOT/current_run_id"
}

run_work_dir() {
  local run_id="$1"
  printf '%s/%s' "$WORK_ROOT_BASE" "$run_id"
}

phase_tc() {
  case "$1" in
    stc) printf '10+0.1' ;;
    ltc) printf '90+0.5' ;;
    *) die "unknown phase: $1" ;;
  esac
}

phase_games_per_pair() {
  case "$1" in
    stc) printf '%s' "$STC_GAMES_PER_PAIR" ;;
    ltc) printf '%s' "$LTC_GAMES_PER_PAIR" ;;
    *) die "unknown phase: $1" ;;
  esac
}

phase_shard_pair_games() {
  case "$1" in
    stc) printf '%s' "$STC_SHARD_PAIR_GAMES" ;;
    ltc) printf '%s' "$LTC_SHARD_PAIR_GAMES" ;;
    *) die "unknown phase: $1" ;;
  esac
}

engine_dir_arg() {
  local dir="$1"
  if [[ "$dir" == "$PIN_ROOT/"* ]]; then
    printf '%s' "${dir#$PIN_ROOT/}"
  else
    printf '%s' "$dir"
  fi
}

shayveri_common_options() {
  [[ -z "$SHAYVERI_OPTIONS" ]] && return 0
  # shellcheck disable=SC2206
  local parts=( $SHAYVERI_OPTIONS )
  printf '%s\n' "${parts[@]}"
}

shayveri_hce_engine_args() {
  printf '%s\n' -engine "name=$HCE_NAME" "cmd=./SHAYVERI" \
    "dir=$(engine_dir_arg "$ENGINES_DIR")" "option.UseNNUE=false"
  shayveri_common_options
}

shayveri_nnue_engine_args() {
  printf '%s\n' -engine "name=$NNUE_NAME" "cmd=./SHAYVERI" \
    "dir=$(engine_dir_arg "$ENGINES_DIR")"
  [[ -z "$NET" ]] || printf '%s\n' "option.EvalFile=$NET"
  shayveri_common_options
}

opponent_engine_args() {
  local opponent="$1"
  case "$opponent" in
    sf2850|SF2850)
      printf '%s\n' -engine "name=SF2850" "cmd=./Stockfish18" \
        "dir=$(engine_dir_arg "$ENGINES_DIR")" "option.UCI_LimitStrength=true" "option.UCI_Elo=2850"
      ;;
    sf3000|SF3000)
      printf '%s\n' -engine "name=SF3000" "cmd=./Stockfish18" \
        "dir=$(engine_dir_arg "$ENGINES_DIR")" "option.UCI_LimitStrength=true" "option.UCI_Elo=3000"
      ;;
    Alexandria9|Berserk13|Ethereal14|PlentyChess7|Weiss2)
      printf '%s\n' -engine "name=$opponent" "cmd=./$opponent" \
        "dir=$(engine_dir_arg "$ENGINES_DIR")"
      ;;
    *)
      die "unknown opponent '$opponent'"
      ;;
  esac
}

tournament_engine_args() {
  shayveri_hce_engine_args
  shayveri_nnue_engine_args

  IFS=',' read -ra opponent_list <<< "$FIXED_OPPONENTS"
  for opponent in "${opponent_list[@]}"; do
    [[ -n "$opponent" ]] || continue
    opponent_engine_args "$opponent"
  done
}
