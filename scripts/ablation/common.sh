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
corrhist_depth_cap|CorrHist_Depth_Cap|0
qs_start_depth|QS_Start_Depth|0
qs_min_depth|QS_Min_Depth|0
rfp_max_depth|RFP_Max_Depth|0
nmp_min_depth|NMP_Min_Depth|0
nmp_reduction_min|NMP_Reduction_Min|0
nmp_reduction_max|NMP_Reduction_Max|0
nmp_verify_min_depth|NMP_Verify_Min_Depth|0
iir_min_depth|IIR_Min_Depth|0
fp_max_depth|FP_Max_Depth|0
see_max_depth|SEE_Max_Depth|0
lmr_min_depth|LMR_Min_Depth|0
lmr_extra_move_threshold|LMR_Extra_Move_Threshold|0
lmr_extra_min_depth|LMR_Extra_Min_Depth|0
lmr_good_history|LMR_Good_History|0
lmr_bad_history|LMR_Bad_History|0
asp_min_depth|ASP_Min_Depth|0
asp_growth|ASP_Growth|2
capture_history_weight|Capture_History_Weight|0
history_max|History_Max|1024
TESTS
}
