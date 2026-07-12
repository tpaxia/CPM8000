#!/bin/sh
#
# regenerate-cpm8k.sh -- STEP 1 of regenerating the CP/M-8000 source tree.
#
# Extracts the pristine distribution files from the six Olivetti-M20 disk
# images (distribution/CPM_8000_1.1/*.IMG) into the target directory, using
# cpmtools with the self-contained m20 diskdef (src/diskdefs_m20.mame). The
# target is replaced with EXACTLY what is on the distribution disks -- no local
# edits, no stray files. This is reproducible: same images -> same tree.
#
# Follow with scripts/overlay-cpm8k.sh (STEP 2) to drop in the toolchain fix.
#
# Usage: scripts/regenerate-cpm8k.sh [target-dir]    (default: src/cpm8k)

set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

OUT=${1:-src/cpm8k}
IMGDIR=distribution/CPM_8000_1.1
IMAGES="REL11A REL11B REL11C GAMES MISC11 TEXT11"
export DISKDEFS=src/diskdefs_m20.mame

command -v cpmcp >/dev/null 2>&1 || { echo "error: cpmtools (cpmcp) not found -- brew install cpmtools" >&2; exit 1; }
for i in $IMAGES; do
	[ -f "$IMGDIR/$i.IMG" ] || { echo "error: missing image $IMGDIR/$i.IMG" >&2; exit 1; }
done

echo "== Step 1: regenerate pristine tree -> $OUT  (from $IMGDIR) =="
TMP=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-regen.XXXXXX")
trap 'rm -rf "$TMP"' EXIT INT TERM

for i in $IMAGES; do
	cpmcp -f m20 "$IMGDIR/$i.IMG" '0:*' "$TMP/"
done
echo "extracted $(ls "$TMP" | wc -l | tr -d ' ') files from $(set -- $IMAGES; echo $#) images"

# Replace the target's top-level files with the freshly-extracted pristine set.
# The tree has no subdirectories; deleting top-level files first drops any local
# strays (e.g. tmptic.z8k) so the result is exactly the distribution.
mkdir -p "$OUT"
find "$OUT" -maxdepth 1 -type f -delete
cp "$TMP"/* "$OUT/"
echo "regenerated $OUT: $(ls "$OUT" | wc -l | tr -d ' ') files (pristine)"
