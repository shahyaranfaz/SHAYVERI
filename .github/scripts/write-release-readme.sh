#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
LICENSE_FILE="$SCRIPT_DIR/../../LICENSE"
CHANGELOG_FILE="$SCRIPT_DIR/../../docs/CHANGELOG.md"

release_metadata() {
  local release_tag=$1

  case "$release_tag" in
    v1.*|v2.0.0|v2.1.0|v2.2.0|v2.3.0|v2.4.0)
      HISTORICAL_RELEASE=true
      ;;
    *)
      HISTORICAL_RELEASE=false
      ;;
  esac

  case "$release_tag" in
    v2.0.0) NETWORK="External NNUE: first_net.nnue" ;;
    v2.1.0|v2.3.0) NETWORK="External NNUE: net5_final.nnue" ;;
    v2.2.0) NETWORK="External NNUE: net8_final.nnue" ;;
    v2.4.0) NETWORK="External NNUE: SHAYVERI2_2_0.nnue" ;;
    v2.5.0) NETWORK="External NNUE: SHAYVERI2_5_0.nnue" ;;
    v2.6.0|v2.7.0) NETWORK="Embedded NNUE: SHAYVERI2_5_0.nnue" ;;
    *) NETWORK="Handcrafted evaluation, no NNUE file required" ;;
  esac

  case "$release_tag" in
    v1.0.0|v1.1.0|v1.2.0)
      BOOK_NOTE="This historical release has an internal opening book without an OwnBook disable option."
      ;;
    v1.3.0|v2.*)
      BOOK_NOTE="For external tournament testing, set OwnBook=false."
      ;;
    *)
      BOOK_NOTE="No release-specific UCI configuration is required."
      ;;
  esac
}

write_release_notes() {
  local output_file=$1
  local release_tag=$2
  local wrap_for_readme=${3:-false}
  local unwrap_changelog=true

  if [[ $wrap_for_readme == true ]]; then
    unwrap_changelog=false
  fi

  release_metadata "$release_tag"

  cat > "$output_file" <<EOF
- **Authors:** Shahyar Anfaz and Averi Wylie
- **License:** GPL-3.0-or-later
- **Source:** https://github.com/shahyaranfaz/SHAYVERI/tree/$release_tag
EOF

  if [[ $HISTORICAL_RELEASE == true ]]; then
    if [[ $wrap_for_readme == true ]]; then
      cat >> "$output_file" <<EOF

This is a historical release. Its version number was assigned retroactively
for consistency with the current scheme, though it was never actually released.
This distinction concerns versioning and release organization and does not
necessarily reflect the rigour of testing performed on the engine.
EOF
    else
      cat >> "$output_file" <<EOF

This is a historical release. Its version number was assigned retroactively for consistency with the current scheme, though it was never actually released. This distinction concerns versioning and release organization and does not necessarily reflect the rigour of testing performed on the engine.
EOF
    fi
  fi

  cat >> "$output_file" <<EOF

## Engine Changes

EOF

  awk -v tag="$release_tag" -v unwrap="$unwrap_changelog" '
    function flush_paragraph() {
      if (paragraph != "") {
        print paragraph
        paragraph=""
      }
    }
    function emit(line) {
      if (unwrap == "true") {
        if (line == "") {
          flush_paragraph()
          pending_blank=1
          return
        }
        if (line ~ /^- / && paragraph != "") flush_paragraph()
        if (pending_blank && started) print ""
        pending_blank=0
        sub(/^[[:space:]]+/, "", line)
        paragraph=paragraph (paragraph == "" ? "" : " ") line
        started=1
        return
      }
      if (!started && line == "") return
      if (line == "") {
        pending_blank=1
        return
      }
      if (pending_blank && started) print ""
      pending_blank=0
      print line
      started=1
    }
    $0 ~ "^### " tag " " { found=1; next }
    found && /^### / { exit }
    found && /^\*\*Default network:\*\*/ { next }
    found { emit($0) }
    END { if (unwrap == "true") flush_paragraph() }
  ' "$CHANGELOG_FILE" >> "$output_file"

  if ! grep -q '^- ' "$output_file"; then
    echo "Changelog entry for $release_tag has no release bullets" >&2
    exit 1
  fi

  cat >> "$output_file" <<EOF

## Release Configuration

- CPU requirement: x86-64-v3 with AVX2 and BMI2
- Protocol: UCI
- Network: $NETWORK
EOF

  if [[ $wrap_for_readme == true && $BOOK_NOTE == "This historical release has an internal opening book without an OwnBook disable option." ]]; then
    cat >> "$output_file" <<EOF
- Opening book: This historical release has an internal opening book without an
  OwnBook disable option.
EOF
  else
    echo "- Opening book: $BOOK_NOTE" >> "$output_file"
  fi

  if [[ $NETWORK == "External NNUE:"* ]]; then
    if [[ $wrap_for_readme == true ]]; then
      cat >> "$output_file" <<EOF

Keep any external NNUE file in the same directory as the engine executable
unless an explicit EvalFile path is configured.
EOF
    else
      cat >> "$output_file" <<EOF

Keep any external NNUE file in the same directory as the engine executable unless an explicit EvalFile path is configured.
EOF
    fi
  fi
}

if [[ ${1:-} == "--notes" ]]; then
  [[ $# -eq 3 ]] || {
    echo "usage: $0 --notes <output-file> <release-tag>" >&2
    exit 1
  }
  write_release_notes "$2" "$3"
  exit 0
fi

[[ $# -eq 3 ]] || {
  echo "usage: $0 <package-dir> <release-tag> <platform>" >&2
  exit 1
}

PACKAGE_DIR=$1
RELEASE_TAG=$2
PLATFORM=$3
NOTES_FILE="$PACKAGE_DIR/release-notes.txt"

write_release_notes "$NOTES_FILE" "$RELEASE_TAG" true
cp "$LICENSE_FILE" "$PACKAGE_DIR/LICENSE"

cat > "$PACKAGE_DIR/README.txt" <<EOF
SHAYVERI $RELEASE_TAG

Platform: $PLATFORM

EOF

cat "$NOTES_FILE" >> "$PACKAGE_DIR/README.txt"
rm "$NOTES_FILE"
