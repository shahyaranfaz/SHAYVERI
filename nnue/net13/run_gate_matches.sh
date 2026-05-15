#!/usr/bin/env bash
set -euo pipefail

# Gate runner for net13. It does not choose a winner; it runs the anchors from
# net13results.txt with identical options and writes PGNs/logs under RESULTS_DIR.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/branch_config.sh"

CUTECHESS="${CUTECHESS:-cutechess-cli}"
RUN_ROOT="${RUN_ROOT:-$PWD}"
ENGINES_DIR="${ENGINES_DIR:-$RUN_ROOT}"
HCE_DIR="${HCE_DIR:-$NFS_ROOT/no_net_dir}"
SHAYVERI_CMD="${SHAYVERI_CMD:-./SHAYVERI}"
HCE_CMD="${HCE_CMD:-$RUN_ROOT/SHAYVERI}"
RESULTS_DIR="${RESULTS_DIR:-$NFS_ROOT/gates}"
BOOK="${BOOK:-books/UHO_2024_8mvs_split_10.epd}"
HASH="${HASH:-64}"
THREADS="${THREADS:-1}"
CONCURRENCY="${CONCURRENCY:-23}"
TC_STC="${TC_STC:-10+0.1}"
TC_LTC="${TC_LTC:-90+0.5}"
GAMES_VS_NET5="${GAMES_VS_NET5:-800}"
GAMES_VS_HCE="${GAMES_VS_HCE:-800}"
GAMES_VS_SF="${GAMES_VS_SF:-800}"
GAMES_LTC="${GAMES_LTC:-240}"

mkdir -p "$RESULTS_DIR"
mkdir -p "$HCE_DIR"

if (( $# < 2 )); then
  echo "usage: $0 <candidate-name> <candidate.nnue> [net5.nnue]" >&2
  exit 2
fi

candidate_name="$1"
candidate_nnue="$2"
net5_nnue="${3:-net5_final.nnue}"

for required in "$candidate_nnue" "$net5_nnue"; do
  if [[ ! -f "$required" ]]; then
    echo "missing net file: $required" >&2
    exit 1
  fi
done
if [[ ! -x "$ENGINES_DIR/SHAYVERI" ]]; then
  echo "missing SHAYVERI in ENGINES_DIR: $ENGINES_DIR/SHAYVERI" >&2
  exit 1
fi
if [[ ! -x "$HCE_CMD" ]]; then
  echo "missing HCE command: $HCE_CMD" >&2
  exit 1
fi

run_match() {
  local name="$1"
  shift
  echo "== $name =="
  "$@" 2>&1 | tee "$RESULTS_DIR/${candidate_name}_${name}.log"
}

# Gate 1: candidate vs net5 at 10+0.1.
run_match "gate1_vs_net5_stc" \
  "$CUTECHESS" \
    -engine name="$candidate_name" dir="$ENGINES_DIR" cmd="$SHAYVERI_CMD" option.OwnBook=false option.EvalFile="$candidate_nnue" \
    -engine name=net5 dir="$ENGINES_DIR" cmd="$SHAYVERI_CMD" option.OwnBook=false option.EvalFile="$net5_nnue" \
    -each proto=uci tc="$TC_STC" option.Threads="$THREADS" option.Hash="$HASH" \
    -games "$GAMES_VS_NET5" \
    -repeat \
    -openings file="$BOOK" format=epd order=random \
    -concurrency "$CONCURRENCY" \
    -draw movenumber=40 movecount=8 score=10 \
    -pgnout "$RESULTS_DIR/${candidate_name}_gate1_vs_net5_stc.pgn"

# Gate 2: candidate vs HCE at 10+0.1.
run_match "gate2_vs_hce_stc" \
  "$CUTECHESS" \
    -engine name="$candidate_name" dir="$ENGINES_DIR" cmd="$SHAYVERI_CMD" option.OwnBook=false option.EvalFile="$candidate_nnue" \
    -engine name=SHAYVERI_HCE dir="$HCE_DIR" cmd="$HCE_CMD" \
    -each proto=uci tc="$TC_STC" option.Threads="$THREADS" option.Hash="$HASH" \
    -games "$GAMES_VS_HCE" \
    -repeat \
    -openings file="$BOOK" format=epd order=random \
    -concurrency "$CONCURRENCY" \
    -draw movenumber=40 movecount=8 score=10 \
    -pgnout "$RESULTS_DIR/${candidate_name}_gate2_vs_hce_stc.pgn"

# Gate 3: candidate vs SF2850 at 10+0.1.
run_match "gate3_vs_sf2850_stc" \
  "$CUTECHESS" \
    -engine name="$candidate_name" dir="$ENGINES_DIR" cmd="$SHAYVERI_CMD" option.OwnBook=false option.EvalFile="$candidate_nnue" \
    -engine name=SF2850 dir="$ENGINES_DIR" cmd=./Stockfish18 option.UCI_LimitStrength=true option.UCI_Elo=2850 \
    -each proto=uci tc="$TC_STC" option.Threads="$THREADS" option.Hash="$HASH" \
    -games "$GAMES_VS_SF" \
    -repeat \
    -openings file="$BOOK" format=epd order=random \
    -concurrency "$CONCURRENCY" \
    -draw movenumber=40 movecount=8 score=10 \
    -pgnout "$RESULTS_DIR/${candidate_name}_gate3_vs_sf2850_stc.pgn"

# Gate 4: candidate vs net5 at 90+0.5.
run_match "gate4_vs_net5_ltc" \
  "$CUTECHESS" \
    -engine name="$candidate_name" dir="$ENGINES_DIR" cmd="$SHAYVERI_CMD" option.OwnBook=false option.EvalFile="$candidate_nnue" \
    -engine name=net5 dir="$ENGINES_DIR" cmd="$SHAYVERI_CMD" option.OwnBook=false option.EvalFile="$net5_nnue" \
    -each proto=uci tc="$TC_LTC" option.Threads="$THREADS" option.Hash="$HASH" \
    -games "$GAMES_LTC" \
    -repeat \
    -openings file="$BOOK" format=epd order=random \
    -concurrency "$CONCURRENCY" \
    -draw movenumber=40 movecount=8 score=10 \
    -pgnout "$RESULTS_DIR/${candidate_name}_gate4_vs_net5_ltc.pgn"
