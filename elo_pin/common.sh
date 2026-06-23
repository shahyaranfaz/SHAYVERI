#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="${SCRIPT_DIR:-$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)}"
PIN_ROOT="${PIN_ROOT:-$SCRIPT_DIR}"
RUN_ID="${RUN_ID:-}"

ENGINES_DIR="${ENGINES_DIR:-$PIN_ROOT/engines}"
BOOK="${BOOK:-$PIN_ROOT/books/UHO_2024_8mvs.epd}"
ANCHORS="${ANCHORS:-$PIN_ROOT/anchors}"
ORDO="${ORDO:-$PIN_ROOT/ordo}"
CUTECHESS="${CUTECHESS:-cutechess-cli}"

TC="${TC:-stc}"
TIMEMARGIN="${TIMEMARGIN:-1000}"
CONCURRENCY="${CONCURRENCY:-23}"
GAMES_PER_PAIR_PER_WORKER="${GAMES_PER_PAIR_PER_WORKER:-40}"
REGISTER_SECONDS="${REGISTER_SECONDS:-300}"
RATING_INTERVAL="${RATING_INTERVAL:-100}"

NAME_ID="${NAME_ID:-SHAYVERI NNUE}"
HCE_NAME="${HCE_NAME:-SHAYVERI v1.0}"
NNUE_NAME="${NNUE_NAME:-$NAME_ID}"
NET="${NET:-}"
SHAYVERI_OPTIONS="${SHAYVERI_OPTIONS:-}"

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

tc_value() {
  case "$TC" in
    stc|STC)
      printf '10+0.1'
      ;;
    ltc|LTC)
      printf '90+0.5'
      ;;
    *)
      die "TC must be stc or ltc"
      ;;
  esac
}

tc_label() {
  case "$TC" in
    stc|STC)
      printf 'stc'
      ;;
    ltc|LTC)
      printf 'ltc'
      ;;
    *)
      die "TC must be stc or ltc"
      ;;
  esac
}

run_dir() {
  local run_id="$1"
  printf '%s/results/%s' "$PIN_ROOT" "$run_id"
}

current_run_id() {
  [[ -f "$PIN_ROOT/current_run_id" ]] || die "no current run id at $PIN_ROOT/current_run_id"
  tr -d '\r\n' < "$PIN_ROOT/current_run_id"
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
  printf '%s\n' -engine "name=$HCE_NAME" "cmd=./SHAYVERI" "dir=$(engine_dir_arg "$ENGINES_DIR")" "option.UseNNUE=false"
  shayveri_common_options
}

shayveri_nnue_engine_args() {
  printf '%s\n' -engine "name=$NNUE_NAME" "cmd=./SHAYVERI" "dir=$(engine_dir_arg "$ENGINES_DIR")"
  case "$NET" in
    "")
      ;;
    *)
      printf '%s\n' "option.EvalFile=$NET"
      ;;
  esac
  shayveri_common_options
}

opponent_engine_args() {
  local opponent="$1"
  case "$opponent" in
    sf2850|SF2850)
      printf '%s\n' -engine "name=SF2850" "cmd=./Stockfish18" "dir=$(engine_dir_arg "$ENGINES_DIR")" "option.UCI_LimitStrength=true" "option.UCI_Elo=2850"
      ;;
    sf3000|SF3000)
      printf '%s\n' -engine "name=SF3000" "cmd=./Stockfish18" "dir=$(engine_dir_arg "$ENGINES_DIR")" "option.UCI_LimitStrength=true" "option.UCI_Elo=3000"
      ;;
    Alexandria9)
      printf '%s\n' -engine "name=Alexandria9" "cmd=./Alexandria9" "dir=$(engine_dir_arg "$ENGINES_DIR")"
      ;;
    Berserk13)
      printf '%s\n' -engine "name=Berserk13" "cmd=./Berserk13" "dir=$(engine_dir_arg "$ENGINES_DIR")"
      ;;
    Ethereal14)
      printf '%s\n' -engine "name=Ethereal14" "cmd=./Ethereal14" "dir=$(engine_dir_arg "$ENGINES_DIR")"
      ;;
    PlentyChess7)
      printf '%s\n' -engine "name=PlentyChess7" "cmd=./PlentyChess7" "dir=$(engine_dir_arg "$ENGINES_DIR")"
      ;;
    Weiss2)
      printf '%s\n' -engine "name=Weiss2" "cmd=./Weiss2" "dir=$(engine_dir_arg "$ENGINES_DIR")"
      ;;
    *)
      die "unknown opponent '$opponent'; edit opponent_engine_args in common.sh"
      ;;
  esac
}

rounds_for_pair_games() {
  local games="$1"
  (( games > 0 )) || die "games must be positive"
  (( games % 2 == 0 )) || die "GAMES_PER_PAIR_PER_WORKER must be even with -games 2 -repeat"
  printf '%s' "$((games / 2))"
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
