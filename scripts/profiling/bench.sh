#!/usr/bin/env bash
set -euo pipefail

# Repeated fixed-workload search benchmark. Use CPU=<core> to pin the engine.
# Set CPU= to leave scheduling unconstrained.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
ENGINE="${ENGINE:-$REPO_ROOT/SHAYVERI}"
CPU="${CPU:-0}"
RUNS="${RUNS:-11}"
THREADS="${THREADS:-1}"
EXPECTED_NODES="${EXPECTED_NODES:-101863}"

if [[ ! -x "$ENGINE" ]]; then
    echo "engine not executable: $ENGINE (build it with: make)" >&2
    exit 1
fi
if ! [[ "$RUNS" =~ ^[1-9][0-9]*$ ]]; then
    echo "RUNS must be a positive integer" >&2
    exit 1
fi
if [[ "$THREADS" != "1" ]]; then
    echo "bench is single-threaded; use baseline.py for Lazy SMP scaling" >&2
    exit 1
fi
if [[ -n "$CPU" ]] && ! command -v taskset >/dev/null; then
    echo "taskset not found; rerun with CPU= to disable pinning" >&2
    exit 1
fi

run_bench() {
    if [[ -n "$CPU" ]]; then
        printf 'setoption name Threads value %s\nbench 16 1 3 default depth\nquit\n' "$THREADS" |
            taskset -c "$CPU" "$ENGINE"
    else
        printf 'setoption name Threads value %s\nbench 16 1 3 default depth\nquit\n' "$THREADS" |
            "$ENGINE"
    fi
}

values="$(mktemp)"
trap 'rm -f "$values" "$values.sorted"' EXIT

printf 'engine=%s threads=%s cpu=%s runs=%s\n' "$ENGINE" "$THREADS" "${CPU:-unconstrained}" "$RUNS"
for ((run = 1; run <= RUNS; ++run)); do
    output="$(run_bench)"
    nodes="$(awk '$1 == "Nodes:" {print $2}' <<< "$output")"
    elapsed="$(awk '$1 == "Time" {print $3}' <<< "$output")"
    nps="$(awk '$1 == "NPS" {print $3}' <<< "$output")"
    if [[ -z "$nodes" || -z "$elapsed" || -z "$nps" ]]; then
        echo "bench output was malformed:" >&2
        echo "$output" >&2
        exit 1
    fi
    if [[ "$nodes" != "$EXPECTED_NODES" ]]; then
        echo "unexpected bench node count: got $nodes, expected $EXPECTED_NODES" >&2
        echo "$output" >&2
        exit 1
    fi
    printf 'run=%02d nodes=%s time_ms=%s nps=%s\n' "$run" "$nodes" "$elapsed" "$nps"
    printf '%s\n' "$nps" >> "$values"
done

sort -n "$values" > "$values.sorted"
count="$(wc -l < "$values.sorted")"
if (( count % 2 )); then
    median="$(sed -n "$(((count + 1) / 2))p" "$values.sorted")"
else
    lower="$(sed -n "$((count / 2))p" "$values.sorted")"
    upper="$(sed -n "$((count / 2 + 1))p" "$values.sorted")"
    median="$(((lower + upper) / 2))"
fi

printf 'median_nps=%s\n' "$median"
