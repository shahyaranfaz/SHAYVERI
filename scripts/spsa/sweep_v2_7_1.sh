#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/SHAYVERI" ]; then
    REPO_ROOT="$SCRIPT_DIR"
else
    REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
fi

FASTCHESS="${FASTCHESS:-fastchess}"
ENGINE="${ENGINE:-./SHAYVERI}"
ENGINE_DIR="${ENGINE_DIR:-.}"
OPENING_FILE="${OPENING_FILE:-../books/final_search_mix_shuf.epd}"
OUT_DIR="${OUT_DIR:-scripts/spsa/output/pass_v2_7_1/sweep}"
SUMMARY="$OUT_DIR/summary.txt"
BASE_TIME_SEC="${BASE_TIME_SEC:-1}"
INC_TIME_SEC="${INC_TIME_SEC:-0.1}"
ROUNDS="${ROUNDS:-200}"
GAMES="${GAMES:-2}"
CONCURRENCY="${CONCURRENCY:-23}"
THREADS="${THREADS:-1}"

cd "$REPO_ROOT"
mkdir -p "$OUT_DIR"
: > "$SUMMARY"

summarize_log() {
    local log="$1"
    awk '
        /^Score of / {
            key = $0
            sub(/:.*/, ":", key)
            if (!(key in seen)) order[++n] = key
            seen[key] = 1
            last[key] = $0
        }
        /^Rank Name/ {
            rank = $0 ORS
            in_rank = 1
            next
        }
        in_rank { rank = rank $0 ORS }
        END {
            print "final pairwise scores:"
            for (i = 1; i <= n; ++i) print last[order[i]]
            print ""
            print "final table:"
            printf "%s", rank
        }
    ' "$log"
}

run_sweep() {
    local id="$1"
    local label="$2"
    shift 2
    local log="$OUT_DIR/${id}.log"
    local pgn="$OUT_DIR/${id}.pgn"

    echo "== $label ==" | tee -a "$SUMMARY"
    if ! "$FASTCHESS" \
        -engine name=base cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci \
        "$@" \
        -each proto=uci tc="${BASE_TIME_SEC}+${INC_TIME_SEC}" timemargin=200 option.Threads="$THREADS" \
        -openings file="$OPENING_FILE" format=epd order=random plies=16 \
        -games "$GAMES" -rounds "$ROUNDS" -repeat \
        -concurrency "$CONCURRENCY" -recover \
        -output format=cutechess \
        -pgnout file="$pgn" min=true \
        -ratinginterval 0 >"$log" 2>&1; then
        echo "fastchess failed. Last output:"
        tail -n 40 "$log"
        return 1
    fi

    summarize_log "$log" | tee -a "$SUMMARY"
    echo "log: $log"
    echo "pgn: $pgn"
    echo
}

run_sweep improving_lmr_reduction \
    "Improving_LMR_Reduction: base=1 vs 0,2,3" \
    -engine name=improve0 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.Improving_LMR_Reduction=0 \
    -engine name=improve2 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.Improving_LMR_Reduction=2 \
    -engine name=improve3 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.Improving_LMR_Reduction=3

run_sweep cutnode_lmr_reduction \
    "CutNode_LMR_Reduction: base=1 vs 0,2,3" \
    -engine name=cutnode0 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.CutNode_LMR_Reduction=0 \
    -engine name=cutnode2 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.CutNode_LMR_Reduction=2 \
    -engine name=cutnode3 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.CutNode_LMR_Reduction=3

run_sweep lmr_pv_offset \
    "LMR_PV_Offset: base=-1 vs -3,-2,0" \
    -engine name=pv_m3 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.LMR_PV_Offset=-3 \
    -engine name=pv_m2 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.LMR_PV_Offset=-2 \
    -engine name=pv_0 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.LMR_PV_Offset=0

run_sweep lmr_nonpv_offset \
    "LMR_NonPV_Offset: base=1 vs 0,2,3" \
    -engine name=nonpv0 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.LMR_NonPV_Offset=0 \
    -engine name=nonpv2 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.LMR_NonPV_Offset=2 \
    -engine name=nonpv3 cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci option.LMR_NonPV_Offset=3

echo "combined summary: $SUMMARY"
