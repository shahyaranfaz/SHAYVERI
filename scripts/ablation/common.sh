#!/usr/bin/env bash
set -euo pipefail

FINAL_GATE_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FINAL_GATE_REPO_ROOT="$(cd "$FINAL_GATE_SCRIPT_DIR/../.." && pwd)"
cd "$FINAL_GATE_REPO_ROOT"

ABLATION_BATCH="${ABLATION_BATCH:-final_gate}"
FINAL_GATE_ROOT="${FINAL_GATE_ROOT:-scripts/ablation/outputs/$ABLATION_BATCH}"
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
BASE_ENGINE="${BASE_ENGINE:-$ENGINE}"
CANDIDATE_ENGINE="${CANDIDATE_ENGINE:-$ENGINE}"
ENGINE_DIR="${ENGINE_DIR:-.}"
if [ "$ABLATION_BATCH" = "v2_7_4_components" ]; then
    DEFAULT_BASE_NAME=legacy
else
    DEFAULT_BASE_NAME=base
fi
BASE_NAME="${BASE_NAME:-$DEFAULT_BASE_NAME}"
OPENING_FILE="${OPENING_FILE:-../books/final_search_mix_shuf.epd}"
CONCURRENCY="${CONCURRENCY:-23}"
THREADS="${THREADS:-1}"
WORKER_NAME="${WORKER_NAME:-$(hostname)-$$}"
POLL_SECONDS="${POLL_SECONDS:-2}"
SUITES="${SUITES:-tc5_005 tc10_01}"

mkdir_shared_dirs() {
    mkdir -p "$JOB_DIR" "$WORKING_DIR" "$RESULTS_DIR"
}

emit_tests() {
    case "$ABLATION_BATCH" in
      spsa_batch1_validation)
        cat <<'TESTS'
batch1_top10|-|ASP_Delta=41,FP_Base=217,FP_Max_Depth=4,FP_Mult=762,IIR_Min_Depth=4,LMP_Base=2,LMP_Mult=1,LMR_Base=1.25954,LMR_Scale=1.80102,QS_Delta_Margin=204,RFP_Margin=68,RFP_Max_Depth=5,SEE_Margin=-161
TESTS
        ;;
      final_gate)
        cat <<'TESTS'
check_extension|-|Check_Extension=0
iir_reduction|-|IIR_Reduction=0
lmr_bad_history_reduction|-|LMR_Bad_History_Reduction=0
lmr_extra_reduction|-|LMR_Extra_Reduction=0
lmr_good_history_reduction|-|LMR_Good_History_Reduction=0
se_extension|-|SE_Extension=0
TESTS
        ;;
      v2_7_4)
        cat <<'TESTS'
multicut_off|-|SE_MultiCut=0
negative_extensions_off|-|SE_Negative_Extensions=0
double_extensions_off|-|SE_Double_Extensions=0
triple_extensions_off|-|SE_Triple_Extensions=0
TESTS
        ;;
      v2_7_4_combined)
        cat <<'TESTS'
combined|-|-
TESTS
        ;;
      v2_7_4_components)
        cat <<'TESTS'
multicut_only|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0
negative_only|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0
double_only|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0
triple_only|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1
large_extensions|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=1
multicut_negative|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0
all_features|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=1,SE_Triple_Extensions=1
TESTS
        ;;
      v2_7_4_sweeps)
        cat <<'TESTS'
multicut_only|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0
negative_tt_n1_cut_n2|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0,SE_Negative_TT_Extension=-1,SE_Negative_Cut_Extension=-2
negative_tt_n2_cut_n2|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0,SE_Negative_TT_Extension=-2,SE_Negative_Cut_Extension=-2
negative_tt_n3_cut_n2|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0,SE_Negative_TT_Extension=-3,SE_Negative_Cut_Extension=-2
negative_tt_n4_cut_n2|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0,SE_Negative_TT_Extension=-4,SE_Negative_Cut_Extension=-2
negative_tt_n3_cut_n1|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0,SE_Negative_TT_Extension=-3,SE_Negative_Cut_Extension=-1
negative_tt_n3_cut_n3|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0,SE_Negative_TT_Extension=-3,SE_Negative_Cut_Extension=-3
negative_tt_n3_cut_n4|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=1,SE_Negative_Extensions=1,SE_Double_Extensions=0,SE_Triple_Extensions=0,SE_Negative_TT_Extension=-3,SE_Negative_Cut_Extension=-4
double_margin_0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=0,SE_Double_Extension=2
double_margin_50|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=50,SE_Double_Extension=2
double_margin_100|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=100,SE_Double_Extension=2
double_margin_150|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=150,SE_Double_Extension=2
double_margin_200|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=200,SE_Double_Extension=2
double_margin_300|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=300,SE_Double_Extension=2
double_extension_1|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=100,SE_Double_Extension=1
double_extension_3|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=1,SE_Triple_Extensions=0,SE_Double_Margin=100,SE_Double_Extension=3
triple_margin_0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=0,SE_Triple_Extension=3
triple_margin_50|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=50,SE_Triple_Extension=3
triple_margin_100|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=100,SE_Triple_Extension=3
triple_margin_150|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=150,SE_Triple_Extension=3
triple_margin_200|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=200,SE_Triple_Extension=3
triple_margin_300|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=300,SE_Triple_Extension=3
triple_margin_400|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=400,SE_Triple_Extension=3
triple_extension_1|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=200,SE_Triple_Extension=1
triple_extension_2|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=200,SE_Triple_Extension=2
triple_extension_4|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=0|SE_MultiCut=0,SE_Negative_Extensions=0,SE_Double_Extensions=0,SE_Triple_Extensions=1,SE_Triple_Margin=200,SE_Triple_Extension=4
TESTS
        ;;
      unswept_search_sweeps)
        cat <<'TESTS'
asp_growth_3|ASP_Growth=2|ASP_Growth=3
asp_growth_4|ASP_Growth=2|ASP_Growth=4
improving_lmr_n3|Improving_LMR_Reduction=-1|Improving_LMR_Reduction=-3
improving_lmr_n2|Improving_LMR_Reduction=-1|Improving_LMR_Reduction=-2
improving_lmr_0|Improving_LMR_Reduction=-1|Improving_LMR_Reduction=0
lmr_pv_n3|LMR_PV_Offset=-1|LMR_PV_Offset=-3
lmr_pv_n2|LMR_PV_Offset=-1|LMR_PV_Offset=-2
lmr_pv_0|LMR_PV_Offset=-1|LMR_PV_Offset=0
lmr_nonpv_0|LMR_NonPV_Offset=1|LMR_NonPV_Offset=0
lmr_nonpv_2|LMR_NonPV_Offset=1|LMR_NonPV_Offset=2
lmr_nonpv_3|LMR_NonPV_Offset=1|LMR_NonPV_Offset=3
nmp_min_depth_2|NMP_Min_Depth=3|NMP_Min_Depth=2
nmp_min_depth_4|NMP_Min_Depth=3|NMP_Min_Depth=4
nmp_min_depth_5|NMP_Min_Depth=3|NMP_Min_Depth=5
nmp_min_depth_6|NMP_Min_Depth=3|NMP_Min_Depth=6
nmp_verify_min_depth_4|NMP_Verify_Min_Depth=8|NMP_Verify_Min_Depth=4
nmp_verify_min_depth_6|NMP_Verify_Min_Depth=8|NMP_Verify_Min_Depth=6
nmp_verify_min_depth_10|NMP_Verify_Min_Depth=8|NMP_Verify_Min_Depth=10
nmp_verify_min_depth_12|NMP_Verify_Min_Depth=8|NMP_Verify_Min_Depth=12
nmp_verify_min_depth_16|NMP_Verify_Min_Depth=8|NMP_Verify_Min_Depth=16
pvs_see_min_depth_1|PVS_SEE_Min_Depth=3|PVS_SEE_Min_Depth=1
pvs_see_min_depth_2|PVS_SEE_Min_Depth=3|PVS_SEE_Min_Depth=2
pvs_see_min_depth_4|PVS_SEE_Min_Depth=3|PVS_SEE_Min_Depth=4
pvs_see_min_depth_5|PVS_SEE_Min_Depth=3|PVS_SEE_Min_Depth=5
pvs_see_min_depth_6|PVS_SEE_Min_Depth=3|PVS_SEE_Min_Depth=6
pvs_see_min_moves_1|PVS_SEE_Min_Moves=2|PVS_SEE_Min_Moves=1
pvs_see_min_moves_3|PVS_SEE_Min_Moves=2|PVS_SEE_Min_Moves=3
pvs_see_min_moves_4|PVS_SEE_Min_Moves=2|PVS_SEE_Min_Moves=4
pvs_see_min_moves_6|PVS_SEE_Min_Moves=2|PVS_SEE_Min_Moves=6
pvs_see_min_moves_8|PVS_SEE_Min_Moves=2|PVS_SEE_Min_Moves=8
qs_start_depth_4|QS_Start_Depth=8|QS_Start_Depth=4
qs_start_depth_6|QS_Start_Depth=8|QS_Start_Depth=6
qs_start_depth_10|QS_Start_Depth=8|QS_Start_Depth=10
qs_start_depth_12|QS_Start_Depth=8|QS_Start_Depth=12
TESTS
        ;;
      *)
        echo "unknown ablation batch: $ABLATION_BATCH" >&2
        return 2
        ;;
    esac
}
