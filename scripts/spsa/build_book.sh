#!/usr/bin/env bash
set -euo pipefail

OUT="${OUT:-../books/final_search_mix_shuf.epd}"
MIN_PLIES="${MIN_PLIES:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/Makefile" ] && [ -d "$SCRIPT_DIR/src" ]; then
  REPO_ROOT="$SCRIPT_DIR"
else
  REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
fi
cd "$REPO_ROOT"

mkdir -p "$(dirname "$OUT")"

if [ "$#" -gt 0 ]; then
  sources=("$@")
elif [ -n "${BOOK_SOURCES:-}" ]; then
  # shellcheck disable=SC2206
  sources=($BOOK_SOURCES)
else
  sources=(../books/*.epd ../books/*.fen)
fi

existing=()
for f in "${sources[@]}"; do
  [ -f "$f" ] && existing+=("$f")
done

if [ "${#existing[@]}" -eq 0 ]; then
  echo "error: no book sources found" >&2
  echo "pass sources explicitly, set BOOK_SOURCES, or put .epd/.fen files in ../books" >&2
  exit 1
fi

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

for f in "${existing[@]}"; do
  awk '
    /^[[:space:]]*$/ { next }
    /^[[:space:]]*#/ { next }
    /^[[:space:]]*;/ { next }
    { print }
  ' "$f" >> "$tmp"
done

if [ -n "$MIN_PLIES" ]; then
  awk -v min="$MIN_PLIES" '
    {
      # FEN has at least 6 fields, with the fullmove number in field 6.
      if (NF >= 6 && $6 + 0 >= min) print
    }
  ' "$tmp" | shuf > "$OUT"
else
  shuf "$tmp" > "$OUT"
fi

echo "wrote $OUT"
wc -l "$OUT"
