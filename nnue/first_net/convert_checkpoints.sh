#!/usr/bin/env bash
set -euo pipefail

JSON_DIR="nnue/first_net/artifacts"
OUT_DIR="nets"
CONVERTER="nnue/convert_marlinflow.py"

mkdir -p "$OUT_DIR"

for i in 000 005 010 015 020 025 030 035 039; do
  python3 "$CONVERTER" \
    "$JSON_DIR/shayveri_v2_iter${i}.json" \
    "$OUT_DIR/iter${i}.nnue"
done

ls -lh "$OUT_DIR"/*.nnue
