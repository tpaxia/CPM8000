#!/bin/sh
#
# overlay-cpm8k.sh -- STEP 2 of regenerating the CP/M-8000 source tree.
#
# Applies the toolchain overlay on top of a pristine tree (produced by
# scripts/regenerate-cpm8k.sh). The only deviation the build needs from the
# stock distribution is the linker: the shipped ld8k works for -w final links
# but fails -r relocatable links in the emulator ("p2 can't open <obj>"), so we
# drop in the from-source rebuild.
#
# The overlay linker is committed as src/linker/ld8k.z8k (a build-once, stable
# binary). Rebuild it with scripts/build-ld8k.sh when src/linker/ld8k.c changes:
#   scripts/build-ld8k.sh /tmp/ld8k && cp /tmp/ld8k/ld8k.z8k src/linker/
#
# Usage: scripts/overlay-cpm8k.sh [target-dir]     (default: src/cpm8k)

set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

OUT=${1:-src/cpm8k}
LD8K=src/linker/ld8k.z8k

[ -d "$OUT" ] || { echo "error: $OUT does not exist -- run scripts/regenerate-cpm8k.sh first" >&2; exit 1; }
[ -f "$LD8K" ] || { echo "error: $LD8K missing -- build it: scripts/build-ld8k.sh /tmp/ld8k && cp /tmp/ld8k/ld8k.z8k src/linker/" >&2; exit 1; }

echo "== Step 2: overlay toolchain fix(es) -> $OUT =="
cp "$LD8K" "$OUT/ld8k.z8k"
echo "  ld8k.z8k <- $LD8K   (from-source linker; fixes the -r relocatable path)"
echo "overlay applied."
