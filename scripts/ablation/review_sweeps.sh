#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

REVIEW_ROOT="${REVIEW_ROOT:-scripts/ablation/outputs/review_sweeps}"
case "$REVIEW_ROOT" in
  /*) ;;
  *) REVIEW_ROOT="$REPO_ROOT/$REVIEW_ROOT" ;;
esac
SHARED_DIR="${SHARED_DIR:-$REVIEW_ROOT/shared}"
case "$SHARED_DIR" in
  /*) ;;
  *) SHARED_DIR="$REPO_ROOT/$SHARED_DIR" ;;
esac

JOB_DIR="$SHARED_DIR/jobs"
WORKING_DIR="$SHARED_DIR/working"
RESULTS_DIR="$SHARED_DIR/results"
MASTER_LOCK="$SHARED_DIR/master.lock"
QUEUE_MARKER="$SHARED_DIR/queue.created"

FASTCHESS="${FASTCHESS:-$HOME/chess_arena/fastchess/fastchess}"
OPENING_FILE="${OPENING_FILE:-$HOME/chess_arena/books/final_search_mix_shuf.epd}"
ENGINE="${ENGINE:-./SHAYVERI}"
ENGINE_DIR="${ENGINE_DIR:-$REPO_ROOT}"
CONCURRENCY="${CONCURRENCY:-23}"
THREADS="${THREADS:-1}"
WORKER_NAME="${WORKER_NAME:-$(hostname)-$$}"
POLL_SECONDS="${POLL_SECONDS:-2}"

mkdir_shared_dirs() {
  mkdir -p "$JOB_DIR" "$WORKING_DIR" "$RESULTS_DIR"
}

emit_candidates() {
  cat <<'CANDIDATES'
qs_futility_50|option.QS_Futility_Margin=50
qs_futility_100|option.QS_Futility_Margin=100
qs_futility_150|option.QS_Futility_Margin=150
qs_futility_200|option.QS_Futility_Margin=200
history_pruning_n250_d4_m6|option.History_Pruning_Threshold=-250 option.History_Pruning_Min_Depth=4 option.History_Pruning_Min_Moves=6
history_pruning_n500_d4_m6|option.History_Pruning_Threshold=-500 option.History_Pruning_Min_Depth=4 option.History_Pruning_Min_Moves=6
history_pruning_n800_d4_m6|option.History_Pruning_Threshold=-800 option.History_Pruning_Min_Depth=4 option.History_Pruning_Min_Moves=6
history_pruning_n1100_d4_m6|option.History_Pruning_Threshold=-1100 option.History_Pruning_Min_Depth=4 option.History_Pruning_Min_Moves=6
iir_reduction_0|option.IIR_Reduction=0
iir_reduction_2|option.IIR_Reduction=2
check_extension_0|option.Check_Extension=0
check_extension_2|option.Check_Extension=2
se_extension_0|option.SE_Extension=0
se_extension_2|option.SE_Extension=2
cutnode_lmr_reduction_0|option.CutNode_LMR_Reduction=0
cutnode_lmr_reduction_1|option.CutNode_LMR_Reduction=1
cutnode_lmr_reduction_2|option.CutNode_LMR_Reduction=2
lmr_extra_reduction_0|option.LMR_Extra_Reduction=0
lmr_extra_reduction_2|option.LMR_Extra_Reduction=2
lmr_good_history_reduction_0|option.LMR_Good_History_Reduction=0
lmr_good_history_reduction_n2|option.LMR_Good_History_Reduction=-2
lmr_bad_history_reduction_0|option.LMR_Bad_History_Reduction=0
lmr_bad_history_reduction_2|option.LMR_Bad_History_Reduction=2
pvs_see_margin_0|option.PVS_SEE_Margin=0
pvs_see_margin_n75|option.PVS_SEE_Margin=-75
pvs_see_margin_n175|option.PVS_SEE_Margin=-175
CANDIDATES
}

require_options() {
  local uci_output option
  uci_output="$(cd "$ENGINE_DIR" && printf 'uci\nquit\n' | "$ENGINE")"
  for option in \
    QS_Futility_Margin \
    History_Pruning_Threshold \
    History_Pruning_Min_Depth \
    History_Pruning_Min_Moves \
    IIR_Reduction \
    Check_Extension \
    SE_Extension \
    CutNode_LMR_Reduction \
    LMR_Extra_Reduction \
    LMR_Good_History_Reduction \
    LMR_Bad_History_Reduction \
    PVS_SEE_Margin; do
    if ! grep -Fq "option name $option " <<< "$uci_output"; then
      echo "missing tuning option: $option" >&2
      return 2
    fi
  done
}

create_queue() {
  mkdir_shared_dirs
  if ! mkdir "$MASTER_LOCK" 2>/dev/null; then
    echo "queue creator already active: $MASTER_LOCK" >&2
    return 1
  fi
  trap 'rmdir "$MASTER_LOCK"' EXIT

  if [ -e "$QUEUE_MARKER" ]; then
    echo "queue already created: $SHARED_DIR"
    return 0
  fi

  local count=0 label options suite base_time increment rounds job_id job_file tmp_file
  while IFS='|' read -r label options; do
    for suite in tc5_005 tc10_01; do
      case "$suite" in
        tc5_005)  base_time=5;  increment=0.05; rounds=100 ;;
        tc10_01) base_time=10; increment=0.1;  rounds=50 ;;
      esac
      job_id="${suite}_${label}"
      job_file="$JOB_DIR/$job_id.job"
      tmp_file="$job_file.tmp.$$"
      printf '%s|%s|%s|%s|%s|%s|%s\n' \
        "$job_id" "$suite" "$base_time" "$increment" "$rounds" "$label" "$options" > "$tmp_file"
      mv "$tmp_file" "$job_file"
      count=$((count + 1))
    done
  done < <(emit_candidates)

  printf '%s\n' "$count" > "$SHARED_DIR/expected_jobs"
  date -u +%Y-%m-%dT%H:%M:%SZ > "$QUEUE_MARKER"
  echo "created $count jobs in $SHARED_DIR"
  echo "start each worker with: $0 worker"
}

run_worker() {
  mkdir_shared_dirs
  if [ ! -x "$FASTCHESS" ]; then
    echo "fastchess not executable: $FASTCHESS" >&2
    return 2
  fi
  if [ ! -f "$OPENING_FILE" ]; then
    echo "opening file not found: $OPENING_FILE" >&2
    return 2
  fi
  require_options

  local safe_worker="${WORKER_NAME//[^A-Za-z0-9_.-]/_}"
  echo "worker=$WORKER_NAME shared=$SHARED_DIR concurrency=$CONCURRENCY"

  local job_file claim_file job_id suite base_time increment rounds label options
  local log pgn status candidate_options
  while true; do
    claim_file=""
    for job_file in "$JOB_DIR"/*.job; do
      [ -e "$job_file" ] || continue
      job_id="$(basename "$job_file" .job)"
      if mv "$job_file" "$WORKING_DIR/${job_id}.${safe_worker}.job" 2>/dev/null; then
        claim_file="$WORKING_DIR/${job_id}.${safe_worker}.job"
        break
      fi
    done

    if [ -z "$claim_file" ]; then
      if [ -e "$QUEUE_MARKER" ] &&
         [ -z "$(find "$JOB_DIR" "$WORKING_DIR" -type f -name '*.job' -print -quit)" ]; then
        echo "queue complete"
        return 0
      fi
      sleep "$POLL_SECONDS"
      continue
    fi

    IFS='|' read -r job_id suite base_time increment rounds label options < "$claim_file"
    log="$RESULTS_DIR/$job_id.txt"
    pgn="$RESULTS_DIR/$job_id.pgn"
    status="$RESULTS_DIR/$job_id.status"
    candidate_options=()
    read -r -a candidate_options <<< "$options"

    echo "[$WORKER_NAME] $job_id tc=${base_time}+${increment} games=$((rounds * 2))"
    if "$FASTCHESS" \
      -engine name=base cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci \
      -engine name="$label" cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci \
        "${candidate_options[@]}" \
      -each proto=uci tc="${base_time}+${increment}" timemargin=100 \
        option.Threads="$THREADS" option.OwnBook=false option.Book_Info_Depth=0 \
      -openings file="$OPENING_FILE" format=epd order=random plies=16 \
      -games 2 -rounds "$rounds" -repeat \
      -concurrency "$CONCURRENCY" -recover \
      -output format=cutechess \
      -pgnout file="$pgn" min=true \
      -ratinginterval 0 > "$log" 2>&1; then
      printf 'ok\n' > "$status"
    else
      printf 'failed\n' > "$status"
      echo "failed: $job_id" >&2
      tail -n 40 "$log" >&2 || true
    fi

    grep -E '^(Score of|Elo difference:|Finished match)' "$log" \
      > "$RESULTS_DIR/$job_id.summary" || true
    rm -f "$claim_file"
  done
}

show_status() {
  mkdir_shared_dirs
  local expected="?" pending working done_count failed
  [ -f "$SHARED_DIR/expected_jobs" ] && expected="$(cat "$SHARED_DIR/expected_jobs")"
  pending="$(find "$JOB_DIR" -type f -name '*.job' | wc -l)"
  working="$(find "$WORKING_DIR" -type f -name '*.job' | wc -l)"
  done_count="$(find "$RESULTS_DIR" -type f -name '*.status' | wc -l)"
  failed="$(find "$RESULTS_DIR" -type f -name '*.status' -exec sh -c \
    'grep -qx failed "$1"' _ {} \; | wc -l)"
  printf 'expected=%s pending=%s working=%s done=%s failed=%s\n' \
    "$expected" "$pending" "$working" "$done_count" "$failed"
}

case "${1:-queue}" in
  queue)  create_queue ;;
  worker) run_worker ;;
  status) show_status ;;
  *)
    echo "usage: $0 [queue|worker|status]" >&2
    exit 2
    ;;
esac
