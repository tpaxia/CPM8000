#!/bin/sh
# Compose one persistent host-backed CP/M development drive for a BIOS package.
#
# The drive holds the common development tree plus the package's BIOS overlay,
# FPE definitions and submit recipes for the package's target CPU.  It does not
# depend on the host: mount it in either hosted emulator, e.g.
#   build/emu/cpm8k-z8001 -d C=dir:drives/dev-plasmo
#
# Usage: scripts/build-development-drive.sh <package>   (src/bios/<package>)
#        -> drives/dev-<package>

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
NAME=${1:?usage: build-development-drive.sh <package>}
BIOS="$ROOT/src/bios/$NAME"
[ -f "$BIOS/Makefile" ] || { echo "error: 'src/bios/$NAME' is not a BIOS package" >&2; exit 2; }

DEST="$ROOT/drives/dev-$NAME"
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-dev-$NAME.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT INT TERM

"$ROOT/scripts/media/stage-development.sh" "$STAGE" "$BIOS"

rm -rf "$DEST"
mkdir -p "$ROOT/drives"
mv "$STAGE" "$DEST"
trap - EXIT INT TERM

echo "$NAME development drive created at $DEST"
