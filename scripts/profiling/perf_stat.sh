#!/usr/bin/env bash
set -euo pipefail

# Hardware-counter view of the same fixed workload as bench.sh.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
ENGINE="${ENGINE:-$REPO_ROOT/SHAYVERI}"
CPU="${CPU:-0}"
RUNS="${RUNS:-7}"
REPEATS="${REPEATS:-25}"
THREADS="${THREADS:-1}"
EVENTS="${EVENTS:-task-clock,cycles,instructions,branches,branch-misses,cache-references,cache-misses}"

if [[ ! -x "$ENGINE" ]]; then
    echo "engine not executable: $ENGINE (build it with: make)" >&2
    exit 1
fi
command -v perf >/dev/null || { echo "perf not found" >&2; exit 1; }
if ! [[ "$RUNS" =~ ^[1-9][0-9]*$ ]]; then
    echo "RUNS must be a positive integer" >&2
    exit 1
fi
if [[ -n "$CPU" ]] && ! command -v taskset >/dev/null; then
    echo "taskset not found; rerun with CPU= to disable pinning" >&2
    exit 1
fi
if ! [[ "$REPEATS" =~ ^[1-9][0-9]*$ ]]; then
    echo "REPEATS must be a positive integer" >&2
    exit 1
fi

input="$(mktemp)"
trap 'rm -f "$input"' EXIT
for ((run = 0; run < REPEATS; ++run)); do
    printf 'setoption name Threads value %s\nbench 16 1 3 default depth\n' "$THREADS" >> "$input"
done
printf 'quit\n' >> "$input"

printf 'engine=%s threads=%s cpu=%s perf_runs=%s repeats_per_run=%s\n' \
    "$ENGINE" "$THREADS" "${CPU:-unconstrained}" "$RUNS" "$REPEATS"
if [[ -n "$CPU" ]]; then
    perf stat -r "$RUNS" -e "$EVENTS" -- \
        bash -c 'taskset -c "$1" "$2" < "$3" > /dev/null' \
        _ "$CPU" "$ENGINE" "$input"
else
    perf stat -r "$RUNS" -e "$EVENTS" -- \
        bash -c '"$1" < "$2" > /dev/null' \
        _ "$ENGINE" "$input"
fi
