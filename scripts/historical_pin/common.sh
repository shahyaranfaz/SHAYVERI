#!/usr/bin/env bash
set -euo pipefail

HISTORICAL_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ELO_PIN_ROOT="${ELO_PIN_ROOT:-$HISTORICAL_SCRIPT_DIR/../elo_pin}"
SCRIPT_DIR="$ELO_PIN_ROOT"
PIN_ROOT="$ELO_PIN_ROOT"

# shellcheck source=../elo_pin/common.sh
source "$ELO_PIN_ROOT/common.sh"
SCRIPT_DIR="$HISTORICAL_SCRIPT_DIR"

HISTORICAL_OUTPUT_DIR="${HISTORICAL_OUTPUT_DIR:-$HISTORICAL_SCRIPT_DIR/outputs}"
HISTORICAL_ENGINES_DIR="${HISTORICAL_ENGINES_DIR:-$HISTORICAL_OUTPUT_DIR/engines}"
REFERENCE_DIR="${REFERENCE_DIR:-$HISTORICAL_OUTPUT_DIR}"
GAMES_DIR="${GAMES_DIR:-$HISTORICAL_OUTPUT_DIR/games}"
RESULTS_DIR="${RESULTS_DIR:-$HISTORICAL_OUTPUT_DIR/results}"
DOWNLOAD_DIR="${DOWNLOAD_DIR:-$HISTORICAL_OUTPUT_DIR/downloads}"

TAGS="${TAGS:-v0.1.0 v0.2.0 v0.3.0 v1.0.0 v1.1.0 v1.2.0 v1.3.0 v2.0.0 v2.1.0 v2.2.0}"
TCS="${TCS:-stc ltc}"
OPENING_SEED="${OPENING_SEED:-2600}"

games_for_tc() {
  case "$1" in
    stc)
      printf '200'
      ;;
    ltc)
      printf '100'
      ;;
    *)
      die "unknown time control '$1'"
      ;;
  esac
}

tc_for_label() {
  case "$1" in
    stc)
      printf '10+0.1'
      ;;
    ltc)
      printf '90+0.5'
      ;;
    *)
      die "unknown time control '$1'"
      ;;
  esac
}

count_games() {
  local pgn="$1"
  grep -c '^\[Event ' "$pgn" 2>/dev/null || true
}
