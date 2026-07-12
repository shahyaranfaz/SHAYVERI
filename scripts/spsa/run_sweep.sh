cd ~/chess_arena/chess_bot

FASTCHESS="${FASTCHESS:-$HOME/chess_arena/fastchess/fastchess}"
OUT="${OUT:-interior_sweeps_10s}"
mkdir -p "$OUT"

run_sweep() {
  label="$1"
  option="$2"
  values="$3"
  log="$OUT/${label}.txt"
  pgn="$OUT/${label}.pgn"

  extra_each_options=()
  if [ -n "${RUN_SWEEP_EACH_OPTIONS:-}" ]; then
    read -r -a extra_each_options <<< "$RUN_SWEEP_EACH_OPTIONS"
  fi

  args=(
    -engine name=base cmd=./SHAYVERI dir=. proto=uci
  )

  i=0
  for value in $values; do
    args+=(
      -engine name="${label}_${i}" cmd=./SHAYVERI dir=. proto=uci
      "option.$option=$value"
    )
    i=$((i + 1))
  done

  "$FASTCHESS" "${args[@]}" \
    -tournament roundrobin \
    -each proto=uci tc=10+0.1 timemargin=100 option.Threads=1 \
      option.OwnBook=false option.Book_Info_Depth=0 \
      "${extra_each_options[@]}" \
    -openings file=../books/final_search_mix_shuf.epd format=epd order=random plies=16 \
    -games 2 -rounds 100 -repeat \
    -concurrency 23 -recover \
    -output format=cutechess \
    -pgnout file="$pgn" min=true \
    -ratinginterval 0 \
    > "$log" 2>&1

  echo "===== $label ====="
  grep -E '^(Score of|Rank Name|Elo difference:|Finished match)' "$log"
}
