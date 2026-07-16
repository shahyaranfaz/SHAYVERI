#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

[[ -n "$RUN_ID" ]] || RUN_ID="$(current_run_id)"
WORK_ROOT="$(run_work_dir "$RUN_ID")"
[[ -f "$WORK_ROOT/config.env" ]] || die "missing run configuration: $WORK_ROOT/config.env"
# shellcheck disable=SC1090
source "$WORK_ROOT/config.env"

count_files() {
  local dir="$1"
  local glob="$2"
  find "$dir" -maxdepth 1 -type f -name "$glob" 2>/dev/null | wc -l
}

count_active_workers() {
  find "$WORK_ROOT"/stc/working "$WORK_ROOT"/ltc/working \
    -maxdepth 1 -type f -name '*.job' -printf '%f\n' 2>/dev/null \
    | sed -E 's/^[^.]+\.//; s/\.job$//' \
    | sort -u \
    | wc -l
}

echo "run_id: $RUN_ID"
echo "release: $RELEASE_ID"
echo "frozen roster:  $(count_files "$WORK_ROOT/workers" '*.worker')"
echo "active workers: $(count_active_workers)"
echo "failed:  $(count_files "$WORK_ROOT/failed" '*.failed')"
echo

printf '%-8s %8s %8s %8s %8s\n' phase shards queue working done
for phase in stc ltc; do
  phase_root="$WORK_ROOT/$phase"
  if [[ -d "$phase_root" ]]; then
    shards="$(< "$phase_root/shard_count")"
    printf '%-8s %8s %8s %8s %8s\n' \
      "$phase" "$shards" \
      "$(count_files "$phase_root/queue" '*.job')" \
      "$(count_files "$phase_root/working" '*.job')" \
      "$(count_files "$phase_root/done" '*.done')"
  else
    printf '%-8s %8s %8s %8s %8s\n' "$phase" 0 0 0 0
  fi
done
