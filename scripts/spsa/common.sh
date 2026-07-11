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

PASS_NAME="${PASS_NAME:-pass_v2_6}"
BATCH_ID="${BATCH_ID:-4}"
BATCH_NAME="${BATCH_NAME:-batch${BATCH_ID}}"

ROOT_DIR="${ROOT_DIR:-scripts/spsa/output/$PASS_NAME}"
SHARED_DIR="${SHARED_DIR:-$ROOT_DIR/shared}"
BATCH_DIR="${BATCH_DIR:-$ROOT_DIR/$BATCH_NAME}"

MASTER_JOBS="${MASTER_JOBS:-6}"
WORKER_NAME="${WORKER_NAME:-$(hostname)-$$}"
WORKER_JOBS="${WORKER_JOBS:-23}"
JOB_CLAIMS="${JOB_CLAIMS:-1}"

OPENING_FILE="${OPENING_FILE:-../books/final_search_mix_shuf.epd}"
FASTCHESS="${FASTCHESS:-$HOME/chess_arena/fastchess/fastchess}"
BASE_TIME_SEC="${BASE_TIME_SEC:-3}"
INC_TIME_SEC="${INC_TIME_SEC:-0.03}"
GAMES_PER_BUDGET="${GAMES_PER_BUDGET:-300}"
BUDGET="${BUDGET:-400}"

case "$BATCH_ID" in
  1)
    PARAM_BATCH="{
'ASP_Delta':           {'init': 36,      'lower': 16,   'upper': 80},
'LMR_Base':            {'init': 1.18093, 'lower': 0.90, 'upper': 1.55},
'LMR_Scale':           {'init': 1.82857, 'lower': 1.40, 'upper': 2.30},
'RFP_Margin':          {'init': 60,      'lower': 30,   'upper': 120},
'FP_Base':             {'init': 220,     'lower': 100,  'upper': 330},
'FP_Mult':             {'init': 693,     'lower': 400,  'upper': 900},
'LMP_Base':            {'init': 3,       'lower': 1,    'upper': 8},
'LMP_Mult':            {'init': 1,       'lower': 1,    'upper': 3},
'SEE_Margin':          {'init': -252,    'lower': -600, 'upper': -40},
'QS_Delta_Margin':     {'init': 150,     'lower': 50,   'upper': 350}
}"
    ;;
  2)
    PARAM_BATCH="{
'NMP_Margin_Mult':     {'init': 18,  'lower': 0,  'upper': 60},
'NMP_Eval_Divisor':    {'init': 200, 'lower': 80, 'upper': 500},
'NMP_Base_Reduction':  {'init': 3,   'lower': 2,  'upper': 5},
'NMP_Depth_Divisor':   {'init': 4,   'lower': 2,  'upper': 8},
'SE_Min_Depth':        {'init': 9,   'lower': 1,  'upper': 16},
'SE_Depth_Margin':     {'init': 2,   'lower': 1,  'upper': 8},
'SE_Margin':           {'init': 58,  'lower': 25, 'upper': 100},
'SE_Reduction_Denom':  {'init': 4,   'lower': 2,  'upper': 6}
}"
    ;;
  3)
    PARAM_BATCH="{
'History_Bonus_Mult':  {'init': 468,  'lower': 280,  'upper': 720},
'History_Bonus_Sub':   {'init': 165,  'lower': 60,   'upper': 340},
'History_Bonus_Limit': {'init': 3090, 'lower': 1500, 'upper': 4500},
'Main_History_Weight': {'init': 85,   'lower': 25,   'upper': 160},
'CMH_Weight':          {'init': 75,   'lower': 15,   'upper': 160},
'FMH_Weight':          {'init': 30,   'lower': 0,    'upper': 130}
}"
    ;;
  4)
    PARAM_BATCH="{
'CorrHist_Scale':       {'init': 256,  'lower': 64,   'upper': 512},
'CorrHist_Bonus_Mult':  {'init': 32,   'lower': 4,    'upper': 128},
'CorrHist_Bonus_Limit': {'init': 768,  'lower': 128,  'upper': 4096},
'CorrHist_Max':         {'init': 8192, 'lower': 2048, 'upper': 32768}
}"
    ;;
  *)
    echo "unknown BATCH_ID=$BATCH_ID" >&2
    exit 2
    ;;
esac

export OPENING_FILE FASTCHESS BASE_TIME_SEC INC_TIME_SEC GAMES_PER_BUDGET
