#!/usr/bin/env bash
set -euo pipefail

# Capture an instruction-level hotspot profile after bench.sh reports a real
# speed change.  View it with: perf report -i "$OUT".
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/../.." && pwd)"
ENGINE="${ENGINE:-$REPO_ROOT/SHAYVERI}"
CPU="${CPU:-0}"
THREADS="${THREADS:-1}"
FREQUENCY="${FREQUENCY:-999}"
REPEATS="${REPEATS:-25}"
OUTPUT_DIR="${OUTPUT_DIR:-$SCRIPT_DIR/outputs}"
OUT="${OUT:-$OUTPUT_DIR/shayveri-bench.perf.data}"
META="${META:-$OUT.meta.txt}"
EXPECTED_NODES="${EXPECTED_NODES:-93023}"
if [[ -z "${EVENT:-}" ]]; then
    if [[ -d /sys/bus/event_source/devices/cpu_core ]]; then
        # Select the P-core PMU explicitly on hybrid Intel CPUs.
        EVENT="cpu_core/cycles/"
    else
        EVENT="cycles"
    fi
fi

if [[ ! -x "$ENGINE" ]]; then
    echo "engine not executable: $ENGINE (build it with: make)" >&2
    exit 1
fi
command -v perf >/dev/null || { echo "perf not found" >&2; exit 1; }
if ! [[ "$REPEATS" =~ ^[1-9][0-9]*$ ]]; then
    echo "REPEATS must be a positive integer" >&2
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

validate_bench() {
    local output nodes
    if [[ -n "$CPU" ]]; then
        output="$(printf 'bench 16 1 3 default depth\nquit\n' | taskset -c "$CPU" "$ENGINE")"
    else
        output="$(printf 'bench 16 1 3 default depth\nquit\n' | "$ENGINE")"
    fi
    nodes="$(awk '$1 == "Nodes:" {print $2}' <<< "$output")"
    if [[ "$nodes" != "$EXPECTED_NODES" ]]; then
        echo "unexpected bench node count: got ${nodes:-missing}, expected $EXPECTED_NODES" >&2
        exit 1
    fi
}

validate_bench

input="$(mktemp)"
trap 'rm -f "$input"' EXIT
mkdir -p "$(dirname -- "$OUT")"
for ((run = 0; run < REPEATS; ++run)); do
    printf 'setoption name Threads value %s\nbench 16 1 3 default depth\n' "$THREADS" >> "$input"
done
printf 'quit\n' >> "$input"

printf 'recording engine=%s threads=%s cpu=%s repeats=%s event=%s output=%s\n' \
    "$ENGINE" "$THREADS" "${CPU:-unconstrained}" "$REPEATS" "$EVENT" "$OUT"
{
    printf 'engine=%s\nthreads=%s\ncpu=%s\nrepeats=%s\nevent=%s\n' \
        "$ENGINE" "$THREADS" "${CPU:-unconstrained}" "$REPEATS" "$EVENT"
    printf 'engine_sha256=%s\n' "$(sha256sum "$ENGINE" | awk '{print $1}')"
    printf 'git_commit=%s\n' "$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || echo unavailable)"
    printf 'git_describe=%s\n' "$(git -C "$REPO_ROOT" describe --always --dirty 2>/dev/null || echo unavailable)"
    printf 'kernel=%s\n' "$(uname -a)"
} > "$META"
if [[ -n "$CPU" ]]; then
    perf record -e "$EVENT" -o "$OUT" -F "$FREQUENCY" -g --call-graph dwarf -- \
        taskset -c "$CPU" "$ENGINE" < "$input" > /dev/null
else
    perf record -e "$EVENT" -o "$OUT" -F "$FREQUENCY" -g --call-graph dwarf -- \
        "$ENGINE" < "$input" > /dev/null
fi

printf 'profile written: %s\nmetadata written: %s\nopen with: perf report -i %s\n' "$OUT" "$META" "$OUT"
