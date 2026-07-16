#!/usr/bin/env bash
set -euo pipefail

SPSA_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ -f "$SPSA_SCRIPT_DIR/SHAYVERI" ] || [ -d "$SPSA_SCRIPT_DIR/src" ]; then
  SPSA_REPO_ROOT="$SPSA_SCRIPT_DIR"
else
  SPSA_REPO_ROOT="$(cd "$SPSA_SCRIPT_DIR/../.." && pwd)"
fi

if [ -f "$SPSA_SCRIPT_DIR/lakas_nnue.py" ]; then
  SPSA_LAKAS_NNUE="$SPSA_SCRIPT_DIR/lakas_nnue.py"
else
  SPSA_LAKAS_NNUE="$SPSA_SCRIPT_DIR/lakas/lakas_nnue.py"
fi

PASS_NAME="${PASS_NAME:-pass_v2_7}"
BATCH_ID="${BATCH_ID:-1}"
BATCH_NAME="${BATCH_NAME:-batch${BATCH_ID}}"

ROOT_DIR="${ROOT_DIR:-scripts/spsa/outputs/$PASS_NAME}"
SHARED_DIR="${SHARED_DIR:-$ROOT_DIR/shared}"
BATCH_DIR="${BATCH_DIR:-$ROOT_DIR/$BATCH_NAME}"

MASTER_JOBS="${MASTER_JOBS:-6}"
WORKER_NAME="${WORKER_NAME:-$(hostname)-$$}"
WORKER_JOBS="${WORKER_JOBS:-23}"
JOB_CLAIMS="${JOB_CLAIMS:-1}"

OPENING_FILE="${OPENING_FILE:-../books/final_search_mix_shuf.epd}"
FASTCHESS="${FASTCHESS:-$HOME/chess_arena/fastchess/fastchess}"
SPSA_ENGINE_OPTIONS="${SPSA_ENGINE_OPTIONS:-option.OwnBook=false option.BookInfoDepth=0}"
GAMES_PER_BUDGET="${GAMES_PER_BUDGET:-300}"
BUDGET="${BUDGET:-400}"

case "$BATCH_ID" in
  1)
    BASE_TIME_SEC="${BASE_TIME_SEC:-5}"
    INC_TIME_SEC="${INC_TIME_SEC:-0.05}"
    PARAM_BATCH="{
'ASP_Delta':         {'init': 41,      'lower': 16,   'upper': 80},
'FP_Base':           {'init': 217,     'lower': 80,   'upper': 350},
'FP_Max_Depth':      {'init': 4,       'lower': 1,    'upper': 12},
'FP_Mult':           {'init': 762,     'lower': 300,  'upper': 1000},
'IIR_Min_Depth':     {'init': 4,       'lower': 1,    'upper': 12},
'LMP_Base':          {'init': 2,       'lower': 1,    'upper': 8},
'LMP_Mult':          {'init': 1,       'lower': 1,    'upper': 3},
'LMR_Base':          {'init': 1.25954, 'lower': 0.90, 'upper': 1.55},
'LMR_Scale':         {'init': 1.80102, 'lower': 1.40, 'upper': 2.30},
'QS_Delta_Margin':   {'init': 204,     'lower': 50,   'upper': 350},
'RFP_Margin':        {'init': 68,      'lower': 30,   'upper': 120},
'RFP_Max_Depth':     {'init': 5,       'lower': 1,    'upper': 12},
'SEE_Margin':        {'init': -161,    'lower': -600, 'upper': -40}
}"
    ;;
  2)
    BASE_TIME_SEC="${BASE_TIME_SEC:-10}"
    INC_TIME_SEC="${INC_TIME_SEC:-0.1}"
    PARAM_BATCH="{
'ASP_Min_Depth':            {'init': 4,    'lower': 1,     'upper': 12},
'CorrHist_Depth_Cap':       {'init': 4,    'lower': 1,     'upper': 32},
'Improving_LMR_Reduction':  {'init': -2,   'lower': -3,    'upper': 0},
'LMR_Bad_History':          {'init': -800, 'lower': -4096, 'upper': 0},
'LMR_Extra_Min_Depth':      {'init': 6,    'lower': 1,     'upper': 16},
'LMR_Extra_Move_Threshold': {'init': 6,    'lower': 1,     'upper': 32},
'LMR_Good_History':         {'init': 800,  'lower': 0,     'upper': 4096},
'LMR_Min_Depth':            {'init': 3,    'lower': 1,     'upper': 12},
'LMR_NonPV_Offset':         {'init': 0,    'lower': 0,     'upper': 3},
'LMR_PV_Offset':            {'init': 0,    'lower': -3,    'upper': 0},
'NMP_Reduction_Min':        {'init': 0,    'lower': 0,     'upper': 16},
'QS_Min_Depth':             {'init': -6,   'lower': -16,   'upper': 0},
'QS_SEE_Margin':            {'init': 100,  'lower': 0,     'upper': 200},
'SEE_Max_Depth':            {'init': 4,    'lower': 1,     'upper': 16}
}"
    ;;
  3)
    BASE_TIME_SEC="${BASE_TIME_SEC:-5}"
    INC_TIME_SEC="${INC_TIME_SEC:-0.05}"
    PARAM_BATCH="{
'NMP_Base_Reduction':   {'init': 3,   'lower': 2,  'upper': 5},
'NMP_Depth_Divisor':    {'init': 3,   'lower': 2,  'upper': 8},
'NMP_Eval_Divisor':     {'init': 183, 'lower': 80, 'upper': 500},
'NMP_Margin_Mult':      {'init': 15,  'lower': 0,  'upper': 60},
'NMP_Reduction_Max':    {'init': 8,   'lower': 2,  'upper': 12},
'NMP_Verify_Min_Depth': {'init': 10,  'lower': 8,  'upper': 20},
'PVS_SEE_Min_Moves':    {'init': 6,   'lower': 1,  'upper': 8},
'SE_Depth_Margin':      {'init': 2,   'lower': 1,  'upper': 8},
'SE_Margin':            {'init': 74,  'lower': 25, 'upper': 100},
'SE_Min_Depth':         {'init': 11,  'lower': 1,  'upper': 16},
'SE_Reduction_Denom':   {'init': 5,   'lower': 2,  'upper': 6}
}"
    ;;
  4)
    BASE_TIME_SEC="${BASE_TIME_SEC:-5}"
    INC_TIME_SEC="${INC_TIME_SEC:-0.05}"
    PARAM_BATCH="{
'CMH_Weight':          {'init': 75,    'lower': 15,   'upper': 160},
'FMH_Weight':          {'init': 30,    'lower': 0,    'upper': 130},
'History_Bonus_Limit': {'init': 3090,  'lower': 1500, 'upper': 4500},
'History_Bonus_Mult':  {'init': 468,   'lower': 280,  'upper': 720},
'History_Bonus_Sub':   {'init': 165,   'lower': 60,   'upper': 340},
'History_Max':         {'init': 16384, 'lower': 1024, 'upper': 32768},
'Main_History_Weight': {'init': 85,    'lower': 25,   'upper': 160}
}"
    ;;
  5)
    BASE_TIME_SEC="${BASE_TIME_SEC:-10}"
    INC_TIME_SEC="${INC_TIME_SEC:-0.1}"
    PARAM_BATCH="{
'ProbCut_Margin':       {'init': 50, 'lower': 0, 'upper': 300},
'ProbCut_Max_Captures': {'init': 3,  'lower': 1, 'upper': 8},
'ProbCut_Min_Depth':    {'init': 5,  'lower': 3, 'upper': 12},
'ProbCut_Reduction':    {'init': 3,  'lower': 1, 'upper': 5}
}"
    ;;
  *)
    echo "unknown BATCH_ID=$BATCH_ID" >&2
    exit 2
    ;;
esac

export OPENING_FILE FASTCHESS SPSA_ENGINE_OPTIONS BASE_TIME_SEC INC_TIME_SEC GAMES_PER_BUDGET
