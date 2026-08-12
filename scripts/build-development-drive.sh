#!/bin/sh
# Compose one persistent target-specific host-backed CP/M development drive.

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
MODEL=${1:?usage: build-development-drive.sh z8001|z8002}

case "$MODEL" in
z8001) BIOS="$ROOT/src/bios/m20" ;;
z8002) BIOS="$ROOT/src/bios/z8002-demo" ;;
*) echo "usage: $0 z8001|z8002" >&2; exit 2 ;;
esac

DEST="$ROOT/drives/dev-$MODEL"
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-dev-$MODEL.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT INT TERM

"$ROOT/scripts/media/stage-development.sh" "$STAGE" "$BIOS" "$MODEL"

rm -rf "$DEST"
mkdir -p "$ROOT/drives"
mv "$STAGE" "$DEST"
trap - EXIT INT TERM

echo "$MODEL development drive created at $DEST"
