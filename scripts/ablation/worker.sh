#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

mkdir_shared_dirs
cd "$FINAL_GATE_REPO_ROOT"

echo "worker=$WORKER_NAME shared=$SHARED_DIR concurrency=$CONCURRENCY"

while true; do
    claimed_file=""
    for job_file in "$JOB_DIR"/*.job; do
        [ -e "$job_file" ] || continue
        job_id="$(basename "$job_file" .job)"
        candidate="$WORKING_DIR/${job_id}.${WORKER_NAME}.job"
        if mv "$job_file" "$candidate" 2>/dev/null; then
            claimed_file="$candidate"
            break
        fi
    done

    if [ -z "$claimed_file" ]; then
        if [ -e "$QUEUE_MARKER" ] && [ -z "$(find "$JOB_DIR" "$WORKING_DIR" -type f -name '*.job' -print -quit)" ]; then
            echo "queue complete"
            exit 0
        fi
        sleep "$POLL_SECONDS"
        continue
    fi

    job_id="$(basename "$claimed_file" .job)"
    IFS='|' read -r suite base_time increment rounds label base_option_spec candidate_option_spec < "$claimed_file"
    log="$RESULTS_DIR/${job_id}.txt"
    pgn="$RESULTS_DIR/${job_id}.pgn"
    status="$RESULTS_DIR/${job_id}.status"

    echo "[$WORKER_NAME] $job_id: tc=${base_time}+${increment} games=$((rounds * 2))"
    base_options=()
    if [ "$base_option_spec" != "-" ]; then
        IFS=',' read -r -a assignments <<< "$base_option_spec"
        for assignment in "${assignments[@]}"; do
            base_options+=("option.$assignment")
        done
    fi
    candidate_options=()
    if [ "$candidate_option_spec" != "-" ]; then
        IFS=',' read -r -a assignments <<< "$candidate_option_spec"
        for assignment in "${assignments[@]}"; do
            candidate_options+=("option.$assignment")
        done
    fi
    if "$FASTCHESS" \
        -engine name="$BASE_NAME" cmd="$BASE_ENGINE" dir="$ENGINE_DIR" proto=uci \
            "${base_options[@]}" \
        -engine name="$label" cmd="$CANDIDATE_ENGINE" dir="$ENGINE_DIR" proto=uci \
            "${candidate_options[@]}" \
        -each proto=uci tc="${base_time}+${increment}" timemargin=100 option.Threads="$THREADS" \
            option.OwnBook=false option.Book_Info_Depth=0 \
        -openings file="$OPENING_FILE" format=epd order=random plies=16 \
        -games 2 -rounds "$rounds" -repeat \
        -concurrency "$CONCURRENCY" -recover \
        -output format=cutechess \
        -pgnout file="$pgn" min=true \
        -ratinginterval 0 > "$log" 2>&1; then
        printf 'ok\n' > "$status"
    else
        printf 'failed\n' > "$status"
        tail -n 30 "$log" >&2 || true
    fi

    grep -E '^(Score of|Elo difference:|Finished match)' "$log" > "$RESULTS_DIR/${job_id}.summary" || true
    rm -f "$claimed_file"
done
