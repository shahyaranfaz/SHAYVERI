#!/usr/bin/env bash
set -euo pipefail

# Convert one Marlinflow net13 checkpoint JSON into classic Chess768 .nnue.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

TRAINER_DIR="${TRAINER_DIR:-$HOME/marlinflow/trainer}"
PYTHON="${PYTHON:-python3}"

if (( $# != 2 )); then
  echo "usage: $0 <train_id_iterNNN> <output.nnue>" >&2
  echo "example: $0 shayveri_v2.13A_iter010 /tmp/iter010.nnue" >&2
  exit 2
fi

train_id="$1"
output="$2"
json="$TRAINER_DIR/nn/${train_id}.json"

if [[ ! -f "$json" ]]; then
  echo "missing checkpoint JSON: $json" >&2
  exit 1
fi

"$PYTHON" "$REPO_ROOT/nnue/convert_marlinflow.py" "$json" "$output" --king-buckets 1
echo "converted $json -> $output"
