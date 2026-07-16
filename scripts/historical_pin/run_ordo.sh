#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

require_file "$ANCHORS"
require_exe "$ORDO"

IFS=' ' read -ra tag_list <<< "$TAGS"
IFS=' ' read -ra tc_list <<< "$TCS"

for tag in "${tag_list[@]}"; do
  for tc_label in "${tc_list[@]}"; do
    reference="$REFERENCE_DIR/reference_${tc_label}.pgn"
    historical="$GAMES_DIR/$tag/$tc_label/historical_games.pgn"
    run_dir="$RESULTS_DIR/$tag/$tc_label"

    require_file "$reference"
    require_file "$historical"
    mkdir -p "$run_dir"

    cat "$reference" "$historical" > "$run_dir/rating_pool.pgn"
    cp "$ANCHORS" "$run_dir/anchors"

    echo "ordo tag=$tag tc=$tc_label"
    (
      cd "$run_dir"
      # shellcheck disable=SC2086
      "$ORDO" \
        -p rating_pool.pgn \
        $ORDO_FLAGS \
        -o results.txt \
        -c results.csv
    ) | tee "$run_dir/ordo.stdout.txt"
  done
done

echo "all historical ratings complete"
