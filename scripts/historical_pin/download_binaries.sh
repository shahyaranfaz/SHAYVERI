#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=common.sh
source "$SCRIPT_DIR/common.sh"

REPOSITORY="${REPOSITORY:-shahyaranfaz/SHAYVERI}"

command -v curl >/dev/null 2>&1 || die "curl is required"
command -v sha256sum >/dev/null 2>&1 || die "sha256sum is required"
command -v tar >/dev/null 2>&1 || die "tar is required"

mkdir -p "$HISTORICAL_ENGINES_DIR" "$DOWNLOAD_DIR"
IFS=' ' read -ra tag_list <<< "$TAGS"

for tag in "${tag_list[@]}"; do
  package="SHAYVERI-${tag}-linux-x64-v3"
  archive="$DOWNLOAD_DIR/${package}.tar.gz"
  checksum="$archive.sha256"
  target="$HISTORICAL_ENGINES_DIR/$tag"
  temporary="$HISTORICAL_ENGINES_DIR/.${tag}.tmp.$$"
  base_url="https://github.com/$REPOSITORY/releases/download/$tag"

  if [[ -x "$target/SHAYVERI" ]]; then
    echo "skip tag=$tag engine=$target/SHAYVERI"
    continue
  fi

  [[ ! -e "$target" ]] || \
    die "$target exists without an executable SHAYVERI binary"

  echo "download tag=$tag"
  curl -fL --retry 3 -o "$archive" "$base_url/${package}.tar.gz"
  curl -fL --retry 3 -o "$checksum" "$base_url/${package}.tar.gz.sha256"

  (
    cd "$DOWNLOAD_DIR"
    sha256sum -c "${package}.tar.gz.sha256"
  )

  mkdir "$temporary"
  trap 'rm -rf "$temporary"' EXIT
  tar -xzf "$archive" -C "$temporary" --strip-components=1

  [[ -f "$temporary/SHAYVERI" ]] || \
    die "release package for $tag does not contain SHAYVERI"

  chmod +x "$temporary/SHAYVERI"
  mv "$temporary" "$target"
  trap - EXIT

  smoke_output="$(printf 'uci\nisready\nquit\n' | "$target/SHAYVERI")"
  grep -q '^uciok$' <<< "$smoke_output" || die "$tag did not return uciok"
  grep -q '^readyok$' <<< "$smoke_output" || die "$tag did not return readyok"

  echo "ready tag=$tag engine=$target/SHAYVERI"
done

echo "all historical binaries ready"
