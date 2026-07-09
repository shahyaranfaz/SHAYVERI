#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

mkdir -p "$SPSA_REPO_ROOT/$ROOT_DIR" "$SPSA_REPO_ROOT/$BATCH_DIR"
cd "$SPSA_REPO_ROOT"

printf 'master pass=%s batch=%s root=%s shared=%s master_jobs=%s tc=%s+%s games=%s fastchess=%s\n' \
  "$PASS_NAME" "$BATCH_NAME" "$ROOT_DIR" "$SHARED_DIR" "$MASTER_JOBS" \
  "$BASE_TIME_SEC" "$INC_TIME_SEC" "$GAMES_PER_BUDGET" "$FASTCHESS"

python3 "$SPSA_LAKAS_NNUE" \
  --distributed-role master \
  --shared-dir "$SHARED_DIR" \
  --budget "$BUDGET" \
  --num_workers "$MASTER_JOBS" \
  --input-data-file "$BATCH_DIR/$BATCH_NAME.dat" \
  --output-data-file "$BATCH_DIR/$BATCH_NAME.dat" \
  --optimizer-log-file "$BATCH_DIR/$BATCH_NAME.log" \
  --input-param "$PARAM_BATCH"
