#!/usr/bin/env bash
set -euo pipefail

FINAL_GATE_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FINAL_GATE_REPO_ROOT="$(cd "$FINAL_GATE_SCRIPT_DIR/../.." && pwd)"
cd "$FINAL_GATE_REPO_ROOT"

FINAL_GATE_ROOT="${FINAL_GATE_ROOT:-scripts/ablation/outputs/final_gate}"
case "$FINAL_GATE_ROOT" in
    /*) ;;
    *) FINAL_GATE_ROOT="$FINAL_GATE_REPO_ROOT/$FINAL_GATE_ROOT" ;;
esac
SHARED_DIR="${SHARED_DIR:-$FINAL_GATE_ROOT/shared}"
case "$SHARED_DIR" in
    /*) ;;
    *) SHARED_DIR="$FINAL_GATE_REPO_ROOT/$SHARED_DIR" ;;
esac
JOB_DIR="$SHARED_DIR/jobs"
WORKING_DIR="$SHARED_DIR/working"
RESULTS_DIR="$SHARED_DIR/results"
MASTER_LOCK="$SHARED_DIR/master.lock"
QUEUE_MARKER="$SHARED_DIR/queue.created"

FASTCHESS="${FASTCHESS:-$HOME/chess_arena/fastchess/fastchess}"
ENGINE="${ENGINE:-./SHAYVERI}"
ENGINE_DIR="${ENGINE_DIR:-.}"
OPENING_FILE="${OPENING_FILE:-../books/final_search_mix_shuf.epd}"
CONCURRENCY="${CONCURRENCY:-23}"
THREADS="${THREADS:-1}"
WORKER_NAME="${WORKER_NAME:-$(hostname)-$$}"
POLL_SECONDS="${POLL_SECONDS:-2}"

mkdir_shared_dirs() {
    mkdir -p "$JOB_DIR" "$WORKING_DIR" "$RESULTS_DIR"
}

emit_tests() {
    cat <<'TESTS'
check_extension|Check_Extension|0
iir_reduction|IIR_Reduction|0
lmr_bad_history_reduction|LMR_Bad_History_Reduction|0
lmr_extra_reduction|LMR_Extra_Reduction|0
lmr_good_history_reduction|LMR_Good_History_Reduction|0
se_extension|SE_Extension|0
TESTS
}
