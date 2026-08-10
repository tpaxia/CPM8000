#!/bin/sh
# Build logical CP/M development media for a target package and format.

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

NAME=${1:?usage: build-media.sh <name> <format> [bios-package]}
FORMAT=${2:?usage: build-media.sh <name> <format> [bios-package]}
BIOSDIR=${3:-src/bios/$NAME}
case "$BIOSDIR" in
	/*) BIOS_ABS=$BIOSDIR ;;
	*)  BIOS_ABS=$ROOT/$BIOSDIR ;;
esac

[ -f "$BIOS_ABS/Makefile" ] || { echo "error: '$BIOSDIR' is not a target package" >&2; exit 2; }
case "$FORMAT" in *[!A-Za-z0-9_-]*|'') echo "error: invalid media format '$FORMAT'" >&2; exit 2 ;; esac

SUPPORTED=$(make -s -C "$BIOS_ABS" media-formats)
case " $SUPPORTED " in
	*" $FORMAT "*) ;;
	*) echo "error: '$BIOSDIR' does not support media format '$FORMAT' (supported: $SUPPORTED)" >&2; exit 2 ;;
esac

CONFIG="$ROOT/src/media/$FORMAT/format.conf"
[ -f "$CONFIG" ] || { echo "error: media format '$FORMAT' has no $CONFIG" >&2; exit 2; }

STAGE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-media-stage.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT INT TERM
OUT="$ROOT/build/media/$NAME/$FORMAT"

"$ROOT/scripts/media/stage-development.sh" "$STAGE" "$BIOS_ABS"
"$ROOT/scripts/media/pack-cpm.sh" "$STAGE" "$CONFIG" "$OUT"
