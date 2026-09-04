#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=datagen_common.sh
source "$SCRIPT_DIR/datagen_common.sh"

init_dirs

rsync_cmd=("$RSYNC")
if [[ -n "$RSYNC_SSH" ]]; then
  rsync_cmd+=(-e "$RSYNC_SSH")
fi

check_bridge_root() {
  if ! "${rsync_cmd[@]}" --list-only "${BRIDGE_DEST%/}/" >/dev/null 2>&1; then
    echo "bridge destination is not reachable or does not exist: ${BRIDGE_DEST%/}/" >&2
    echo "create it first through the reverse SSH tunnel, for example:" >&2
    echo "  ssh -i ~/.ssh/net15_wsl_bridge -p 2222 shahy@localhost 'mkdir -p ~/shayveri_v3_datagen/bullet'" >&2
    exit 1
  fi
}

transfer_shard() {
  local shard_dir="$1"
  local shard_name
  local claimed_dir
  local dest

  shard_name="$(basename "$shard_dir")"
  claimed_dir="$READY_ROOT/${shard_name}.transfer"
  dest="${BRIDGE_DEST%/}/$shard_name/"

  if ! mv "$shard_dir" "$claimed_dir" 2>/dev/null; then
    return 0
  fi

  if [[ ! -f "$claimed_dir/DONE" ]]; then
    echo "claimed shard missing DONE, leaving for inspection: $claimed_dir" >&2
    return 1
  fi

  echo "== transfer $shard_name to $dest =="
  # shellcheck disable=SC2086
  "${rsync_cmd[@]}" $RSYNC_ARGS --exclude=/DONE "$claimed_dir/" "$dest"
  # Publish the completion marker only after every shard artifact has arrived.
  # shellcheck disable=SC2086
  "${rsync_cmd[@]}" $RSYNC_ARGS "$claimed_dir/DONE" "$dest/DONE"

  if ! "${rsync_cmd[@]}" --list-only "$dest/DONE" >/dev/null 2>&1; then
    echo "bridge verification failed: $dest/DONE not visible" >&2
    mv "$claimed_dir" "$shard_dir"
    return 1
  fi

  echo "$shard_name" > "$STATE_DIR/latest_bridged_shard.txt"
  rm -rf "$claimed_dir"
  echo "deleted server copy $claimed_dir"
}

check_bridge_root

while true; do
  shopt -s nullglob
  ready_shards=("$READY_ROOT"/*_shard_*)
  shopt -u nullglob

  did_work=0
  for shard_dir in "${ready_shards[@]}"; do
    [[ -d "$shard_dir" ]] || continue
    [[ "$shard_dir" != *.transfer ]] || continue
    transfer_shard "$shard_dir"
    did_work=1
  done

  if (( did_work == 0 )); then
    echo "no ready shards; sleeping ${SLEEP_SECONDS}s"
  fi
  sleep "$SLEEP_SECONDS"
done
