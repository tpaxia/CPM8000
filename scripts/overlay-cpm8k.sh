#!/bin/sh
#
# overlay-cpm8k.sh -- STEP 2 of regenerating the CP/M-8000 source tree.
#
# Applies the maintained-source overlay on top of a pristine tree (produced by
# scripts/regenerate-cpm8k.sh).  It installs the from-source linker needed for
# relocatable links and the reconstructed FPE sources/submit that reproduce the
# floating-point objects shipped without source in the distribution.
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
FPEDIR=src/fpe
FPESUB=scripts/fpe.sub

[ -d "$OUT" ] || { echo "error: $OUT does not exist -- run scripts/regenerate-cpm8k.sh first" >&2; exit 1; }
[ -f "$LD8K" ] || { echo "error: $LD8K missing -- build it: scripts/build-ld8k.sh /tmp/ld8k && cp /tmp/ld8k/ld8k.z8k src/linker/" >&2; exit 1; }
for file in fpe.z8k fpedep.z8k biosdefs.z8k; do
	[ -f "$FPEDIR/$file" ] || { echo "error: missing $FPEDIR/$file" >&2; exit 1; }
done
[ -f "$FPESUB" ] || { echo "error: missing $FPESUB" >&2; exit 1; }

echo "== Step 2: overlay toolchain fix(es) -> $OUT =="
cp "$LD8K" "$OUT/ld8k.z8k"
echo "  ld8k.z8k <- $LD8K   (from-source linker; fixes the -r relocatable path)"
cp "$FPEDIR/fpe.z8k" "$OUT/fpe.8kn"
cp "$FPEDIR/fpedep.z8k" "$OUT/fpedep.8kn"
cp "$FPEDIR/biosdefs.z8k" "$OUT/biosdefs.z8k"
cp "$FPESUB" "$OUT/fpe.sub"
echo "  fpe.8kn, fpedep.8kn, biosdefs.z8k, fpe.sub <- maintained FPE sources"
echo "overlay applied."
