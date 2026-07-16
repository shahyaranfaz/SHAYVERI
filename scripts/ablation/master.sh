#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

mkdir_shared_dirs

uci_output="$(cd "$ENGINE_DIR" && printf 'uci\nquit\n' | "$CANDIDATE_ENGINE")"
while IFS='|' read -r _label base_option_spec candidate_option_spec; do
    for option_spec in "$base_option_spec" "$candidate_option_spec"; do
        [ "$option_spec" = "-" ] && continue
        IFS=',' read -r -a assignments <<< "$option_spec"
        for assignment in "${assignments[@]}"; do
            option="${assignment%%=*}"
            if ! grep -Fq "option name $option " <<< "$uci_output"; then
                echo "missing tuning option: $option" >&2
                exit 2
            fi
        done
    done
done < <(emit_tests)

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
while IFS='|' read -r label base_option_spec candidate_option_spec; do
    for suite in $SUITES; do
        case "$suite" in
            tc5_005) base_time=5; increment=0.05; rounds="${ROUNDS_5_005:-100}" ;;
            tc10_01) base_time=10; increment=0.1; rounds="${ROUNDS_10_01:-50}" ;;
        esac

        job_id="${suite}_${label}"
        job_file="$JOB_DIR/$job_id.job"
        tmp_file="$job_file.tmp.$$"
        printf '%s|%s|%s|%s|%s|%s|%s\n' \
            "$suite" "$base_time" "$increment" "$rounds" "$label" \
            "$base_option_spec" "$candidate_option_spec" > "$tmp_file"
        mv "$tmp_file" "$job_file"
        job_count=$((job_count + 1))
    done
done < <(emit_tests)

printf '%s\n' "$job_count" > "$SHARED_DIR/expected_jobs"
printf '%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" > "$QUEUE_MARKER"

echo "created $job_count jobs"
echo "shared root: $SHARED_DIR"
echo "run workers with: $SCRIPT_DIR/worker.sh"
