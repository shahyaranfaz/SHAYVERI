#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

cd "$SPSA_REPO_ROOT"

count_files() {
  local dir="$1"
  if [ -d "$dir" ]; then
    find "$dir" -type f | wc -l
  else
    printf '0\n'
  fi
}

size_of() {
  local path="$1"
  if [ -e "$path" ]; then
    du -h "$path" | awk '{print $1}'
  else
    printf '-'
  fi
}

printf '== spsa %s / %s ==\n' "$PASS_NAME" "$BATCH_NAME"
printf '%-16s %s\n' "shared:" "$SHARED_DIR"
printf '%-16s %s\n' "batch_dir:" "$BATCH_DIR"
printf '%-16s %s\n' "tc:" "${BASE_TIME_SEC}+${INC_TIME_SEC}"
printf '%-16s %s\n' "games/eval:" "$GAMES_PER_BUDGET"
printf '%-16s %s\n' "budget:" "$BUDGET"
echo

printf '%-12s %8s\n' "queue" "files"
for name in jobs working results state lock; do
  printf '%-12s %8s\n' "$name" "$(count_files "$SHARED_DIR/.lakas_dist/$name")"
done
echo

printf '%-12s %-8s %s\n' "artifact" "size" "path"
for ext in dat log html; do
  path="$BATCH_DIR/$BATCH_NAME.$ext"
  printf '%-12s %-8s %s\n' "$ext" "$(size_of "$path")" "$path"
done

