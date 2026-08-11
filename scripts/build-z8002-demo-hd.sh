#!/bin/sh
# Build the target-specific Z8002-demo ATA image and uncompressed MAME CHD.

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

OUT=build/media/z8002-demo/z8002-demo-hd
SYSTEM=build/system/z8002-demo/z8002-demo.boot
FILESYSTEM=$OUT/development.img
RAW=$OUT/z8002-demo.raw
CHD=$OUT/z8002-demo.chd
DISKDEFS=src/diskdefs_z8002_demo

make system NAME=z8002-demo
make media NAME=z8002-demo FORMAT=z8002-demo-hd

[ "$(wc -c < "$SYSTEM" | tr -d ' ')" -eq 65536 ] || {
	echo "error: system prefix is not exactly 64 KiB" >&2
	exit 1
}
[ "$(wc -c < "$FILESYSTEM" | tr -d ' ')" -eq 8323072 ] || {
	echo "error: filesystem is not exactly 8 MiB - 64 KiB" >&2
	exit 1
}

dd if=/dev/zero of="$RAW" bs=8388608 count=1 2>/dev/null
dd if="$SYSTEM" of="$RAW" conv=notrunc 2>/dev/null
dd if="$FILESYSTEM" of="$RAW" bs=512 seek=128 conv=notrunc 2>/dev/null

CHECK=$(mktemp -d "${TMPDIR:-/tmp}/z8002-demo-check.XXXXXX")
trap 'rm -rf "$CHECK"' EXIT INT TERM
cp "$DISKDEFS" "$CHECK/diskdefs"
(cd "$CHECK" && fsck.cpm -f z8002demohd "$ROOT/$FILESYSTEM")

CHDMAN=${CHDMAN:-chdman}
command -v "$CHDMAN" >/dev/null 2>&1 || {
	echo "error: chdman not found; set CHDMAN=/path/to/chdman" >&2
	exit 1
}
"$CHDMAN" createhd -f -i "$RAW" -o "$CHD" -c none -chs 32,16,32 -ss 512
"$CHDMAN" extracthd -i "$CHD" -o "$CHECK/extracted.raw" >/dev/null
cmp -s "$RAW" "$CHECK/extracted.raw" || {
	echo "error: CHD extraction does not reproduce the raw disk" >&2
	exit 1
}

echo "Z8002-demo media created:"
ls -lh "$RAW" "$CHD"
echo "CHD round-trip verified"
