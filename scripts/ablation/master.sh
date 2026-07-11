#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

mkdir_shared_dirs

if ! mkdir "$MASTER_LOCK" 2>/dev/null; then
    echo "master already active: $MASTER_LOCK" >&2
    exit 1
fi
trap 'rmdir "$MASTER_LOCK"' EXIT

if [ -e "$QUEUE_MARKER" ]; then
    echo "queue already created: $SHARED_DIR"
    exit 0
fi

job_count=0
while IFS='|' read -r label option value; do
    for suite in tc5_005 tc10_01; do
        case "$suite" in
            tc5_005) base_time=5; increment=0.05; rounds=100 ;;
            tc10_01) base_time=10; increment=0.1; rounds=50 ;;
        esac

        job_id="${suite}_${label}"
        job_file="$JOB_DIR/$job_id.job"
        tmp_file="$job_file.tmp.$$"
        printf '%s|%s|%s|%s|%s|%s|%s\n' \
            "$suite" "$base_time" "$increment" "$rounds" "$label" "$option" "$value" > "$tmp_file"
        mv "$tmp_file" "$job_file"
        job_count=$((job_count + 1))
    done
done < <(emit_tests)

printf '%s\n' "$job_count" > "$SHARED_DIR/expected_jobs"
printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$QUEUE_MARKER"

echo "created $job_count jobs"
echo "shared root: $SHARED_DIR"
echo "run workers with: $SCRIPT_DIR/worker.sh"
