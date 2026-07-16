#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

candidate_engine_args() {
  local tag="$1"
  local engine_dir="$HISTORICAL_ENGINES_DIR/$tag"
  local engine="$engine_dir/SHAYVERI"

  [[ -x "$engine" ]] || die "missing executable: $engine"

  printf '%s\n' \
    -engine \
    "name=SHAYVERI $tag" \
    "cmd=./SHAYVERI" \
    "dir=$engine_dir"

  case "$tag" in
    v1.0.0|v1.1.0|v1.2.0)
      ;;
    *)
      printf '%s\n' "option.OwnBook=false"
      ;;
  esac
}

validate_log() {
  local log="$1"

  [[ -f "$log" ]] || die "missing fastchess log: $log"

  if grep -Eiq \
    '^warning;.*(illegal move|disconnect)|forfeit on time|lost on time|(illegal moves|disconnects?|timeouts|crashed):[[:space:]]*[1-9]' \
    "$log"; then
    die "fastchess reported a failure in $log"
  fi
}

run_pairing() {
  local tag="$1"
  local tc_label="$2"
  local opponent="$3"
  local games="$4"
  local tc_value="$5"
  local run_dir="$GAMES_DIR/$tag/$tc_label"
  local pgn="$run_dir/${opponent}.pgn"
  local log="$run_dir/${opponent}.log"
  local rounds=$((games / 2))
  local existing=0
  local engine_args=()

  mkdir -p "$run_dir"

  if [[ -f "$pgn" ]]; then
    existing="$(count_games "$pgn")"
  fi

  if (( existing == games )); then
    validate_log "$log"
    echo "skip tag=$tag tc=$tc_label opponent=$opponent games=$existing"
    return
  fi

  if (( existing != 0 )); then
    die "$pgn contains $existing games, expected $games; remove it to rerun"
  fi

  [[ -x "$HISTORICAL_ENGINES_DIR/$tag/SHAYVERI" ]] || \
    die "missing executable: $HISTORICAL_ENGINES_DIR/$tag/SHAYVERI"

  mapfile -t engine_args < <(
    candidate_engine_args "$tag"
    opponent_engine_args "$opponent"
  )

  echo "start tag=$tag tc=$tc_label opponent=$opponent games=$games"
  (
    cd "$ELO_PIN_ROOT"
    "$FASTCHESS" \
      "${engine_args[@]}" \
      -each proto=uci "tc=$tc_value" "timemargin=$TIMEMARGIN" \
        option.Threads=1 \
      -openings "file=$BOOK" format=epd order=random plies=16 \
      -srand "$OPENING_SEED" \
      -games 2 -rounds "$rounds" -repeat \
      -concurrency "$CONCURRENCY" \
      -recover \
      -pgnout "file=$pgn" min=true \
      -scoreinterval "$games" \
      -ratinginterval 0
  ) > "$log" 2>&1

  existing="$(count_games "$pgn")"
  (( existing == games )) || die "$pgn contains $existing games, expected $games"

  validate_log "$log"

  echo "done tag=$tag tc=$tc_label opponent=$opponent games=$existing"
}

combine_pairings() {
  local tag="$1"
  local tc_label="$2"
  local games_per_pairing="$3"
  local run_dir="$GAMES_DIR/$tag/$tc_label"
  local combined="$run_dir/historical_games.pgn"
  local expected_total=0
  local actual_total=0
  local pgns=()
  local opponent

  IFS=',' read -ra opponent_list <<< "$FIXED_OPPONENTS"
  for opponent in "${opponent_list[@]}"; do
    pgns+=("$run_dir/${opponent}.pgn")
  done

  expected_total=$((${#pgns[@]} * games_per_pairing))
  cat "${pgns[@]}" > "$combined"
  actual_total="$(count_games "$combined")"

  (( actual_total == expected_total )) || \
    die "$combined contains $actual_total games, expected $expected_total"

  echo "combined tag=$tag tc=$tc_label games=$actual_total pgn=$combined"
}

require_file "$BOOK"
require_exe "$FASTCHESS"

IFS=' ' read -ra tag_list <<< "$TAGS"
IFS=' ' read -ra tc_list <<< "$TCS"

for tag in "${tag_list[@]}"; do
  for tc_label in "${tc_list[@]}"; do
    games="$(games_for_tc "$tc_label")"
    tc_value="$(tc_for_label "$tc_label")"
    IFS=',' read -ra opponent_list <<< "$FIXED_OPPONENTS"

    for opponent in "${opponent_list[@]}"; do
      run_pairing "$tag" "$tc_label" "$opponent" "$games" "$tc_value"
    done

    combine_pairings "$tag" "$tc_label" "$games"
  done
done

echo "all historical games complete"
