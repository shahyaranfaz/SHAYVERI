#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

mkdir_shared_dirs
expected="?"
[ -f "$SHARED_DIR/expected_jobs" ] && expected="$(cat "$SHARED_DIR/expected_jobs")"

pending=$(find "$JOB_DIR" -type f -name '*.job' 2>/dev/null | wc -l)
working=$(find "$WORKING_DIR" -type f -name '*.job' 2>/dev/null | wc -l)
done_count=$(find "$RESULTS_DIR" -type f -name '*.status' 2>/dev/null | wc -l)
failed=$(find "$RESULTS_DIR" -type f -name '*.status' -exec sh -c 'grep -qx failed "$1"' _ {} \; 2>/dev/null | wc -l)

printf 'expected=%s pending=%s working=%s done=%s failed=%s\n' \
    "$expected" "$pending" "$working" "$done_count" "$failed"
