#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
SOURCE_MANIFEST="$SOURCE_ROOT/SOURCE-FILES-SHA256.txt"
MANIFEST_TMP="$(mktemp /tmp/mobilegl-pz-source-manifest.XXXXXX)"
trap 'rm -f -- "$MANIFEST_TMP"' EXIT

(
    cd "$SOURCE_ROOT"
    find . \
        \( -path './.git' -o -path './build-*' -o \
           -path './cmake-build-*' \) -prune -o \
        -type f ! -name 'SOURCE-FILES-SHA256.txt' -print0 |
        LC_ALL=C sort -z |
        xargs -0 sha256sum
) > "$MANIFEST_TMP"

mv -- "$MANIFEST_TMP" "$SOURCE_MANIFEST"
trap - EXIT
echo "Updated ${SOURCE_MANIFEST#"$SOURCE_ROOT/"}"
