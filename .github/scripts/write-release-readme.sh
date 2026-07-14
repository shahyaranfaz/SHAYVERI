#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR=$1
RELEASE_TAG=$2
PLATFORM=$3

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LICENSE_FILE="$SCRIPT_DIR/../../LICENSE"

case "$RELEASE_TAG" in
  v2.0.0) NETWORK="External NNUE: first_net.nnue" ;;
  v2.1.0|v2.3.0) NETWORK="External NNUE: net5_final.nnue" ;;
  v2.2.0|v2.2.1) NETWORK="External NNUE: net8_final.nnue" ;;
  v2.4.0|v2.4.1) NETWORK="External NNUE: SHAYVERI2_2_0.nnue" ;;
  v2.5.0) NETWORK="External NNUE: SHAYVERI2_5_0.nnue" ;;
  v2.6.0|v2.7.0) NETWORK="Embedded NNUE: SHAYVERI2_5_0" ;;
  *) NETWORK="Handcrafted evaluation; no NNUE file required" ;;
esac

case "$RELEASE_TAG" in
  v0.2.0|v0.3.0|v0.4.0|v1.0.0|v1.1.0|v1.2.0)
    BOOK_NOTE="This historical release has an internal opening book without an OwnBook disable option."
    ;;
  v1.3.0|v1.3.1|v2.*)
    BOOK_NOTE="For external tournament testing, set OwnBook=false."
    ;;
  *)
    BOOK_NOTE="No release-specific UCI configuration is required."
    ;;
esac

cp "$LICENSE_FILE" "$PACKAGE_DIR/LICENSE"

cat > "$PACKAGE_DIR/README.txt" <<EOF
SHAYVERI $RELEASE_TAG

Platform: $PLATFORM
CPU requirement: x86-64-v3 with AVX2 and BMI2
Protocol: UCI
Authors: Shahyar Anfaz and Averi Wylie
License: GPL-3.0-or-later
Source: https://github.com/shahyaranfaz/SHAYVERI/tree/$RELEASE_TAG

$NETWORK
$BOOK_NOTE

Basic UCI usage:
  uci
  isready
  position startpos
  go movetime 1000
  quit

Keep any external NNUE file in the same directory as the engine executable
unless an explicit EvalFile path is configured.
EOF
