#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

cd "$SPSA_REPO_ROOT"

printf 'worker pass=%s root=%s shared=%s worker=%s job_claims=%s fastchess_concurrency=%s tc=%s+%s games=%s fastchess=%s\n' \
  "$PASS_NAME" "$ROOT_DIR" "$SHARED_DIR" "$WORKER_NAME" "$JOB_CLAIMS" "$WORKER_JOBS" \
  "$BASE_TIME_SEC" "$INC_TIME_SEC" "$GAMES_PER_BUDGET" "$FASTCHESS"

python3 "$SPSA_LAKAS_NNUE" \
  --distributed-role worker \
  --shared-dir "$SHARED_DIR" \
  --worker-name "$WORKER_NAME" \
  --num_workers "$JOB_CLAIMS" \
  --concurrency "$WORKER_JOBS" \
  --input-param "{}"
