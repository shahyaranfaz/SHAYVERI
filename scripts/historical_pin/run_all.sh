#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

PYTHON="${PYTHON:-python3}"

parser_args=()
if [[ -n "${STC_SOURCE_PGN:-}" ]]; then
  parser_args+=(--stc-input "$STC_SOURCE_PGN")
fi
if [[ -n "${LTC_SOURCE_PGN:-}" ]]; then
  parser_args+=(--ltc-input "$LTC_SOURCE_PGN")
fi

"$HISTORICAL_SCRIPT_DIR/download_binaries.sh"
"$PYTHON" "$HISTORICAL_SCRIPT_DIR/parse_games.py" "${parser_args[@]}"
"$HISTORICAL_SCRIPT_DIR/run_games.sh"
"$HISTORICAL_SCRIPT_DIR/run_ordo.sh"

echo "historical pin pipeline complete"
