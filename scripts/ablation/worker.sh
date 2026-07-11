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
    IFS='|' read -r suite base_time increment rounds label option value < "$claimed_file"
    log="$RESULTS_DIR/${job_id}.txt"
    pgn="$RESULTS_DIR/${job_id}.pgn"
    status="$RESULTS_DIR/${job_id}.status"

    echo "[$WORKER_NAME] $job_id: $option=$value tc=${base_time}+${increment} games=$((rounds * 2))"
    if "$FASTCHESS" \
        -engine name=base cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci \
        -engine name="$label" cmd="$ENGINE" dir="$ENGINE_DIR" proto=uci \
            "option.$option=$value" \
        -each proto=uci tc="${base_time}+${increment}" timemargin=100 option.Threads="$THREADS" \
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
