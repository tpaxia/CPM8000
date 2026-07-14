#!/bin/sh
#
# build-fpe.sh -- assemble the CP/M-8000 floating-point library (fpe.o,
# fpedep.o) from source (src/fpe) using the emulator's in-guest assembler.
#
# The M20 has no Z8070 coprocessor, so floating point is emulated in software:
# fpe.z8k is the Z8000 EPA extended-instruction trap handler / emulator, and
# fpedep.z8k is its system-dependent half. Both are assembled with asz8k (the
# distribution assembler; the emulated FP instructions need V1.1B FP support)
# then converted by xcon. fpe.z8k .inputs biosdefs.z8k; fpedep.z8k .inputs
# biosdefs.z8k (both halves share it).
#
# NOTE on reproduction vs the distribution objects:
#  - fpe.o is functionally identical to src/cpm8k/fpe.o; the only byte
#    differences are in the epuwp work area, declared with .block (reserved,
#    uninitialized) -- the distribution binary carries leftover buffer garbage
#    there, this build emits clean zeros. No relocations touch that region.
#  - fpedep.o object content is byte-identical to src/cpm8k/fpedep.o (the
#    function bodies were transcribed from the distribution disassembly); only
#    trailing file padding differs.
#
# Usage: scripts/build-fpe.sh [output-dir]     (default: build/fpe)

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU=build/emu/cpm8k
SRC=src/cpm8k
FSRC=src/fpe
SUB=scripts/fpe.sub
OUT=${1:-build/fpe}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f build/bios-emu/cpm.sys ] || { echo "error: build/bios-emu/cpm.sys missing -- run 'make bios-emu' first" >&2; exit 1; }

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-fpe.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
# main sources -> .8kn (asz8k requires a .8k{n,s} main file); includes stay .z8k
cp "$FSRC/fpe.z8k"    "$DRIVE/fpe.8kn"
cp "$FSRC/fpedep.z8k" "$DRIVE/fpedep.8kn"
cp "$FSRC/biosdefs.z8k" "$DRIVE/"
# assembler + converter + predef (distribution asz8k, which supports the FP ops)
cp "$SRC/asz8k.z8k" "$DRIVE/ASZ8K.Z8K"
cp "$SRC/xcon.z8k"  "$DRIVE/"
cp "$SRC/asz8k.pd"  "$DRIVE/asz8k.pd"
cp "$SUB" "$DRIVE/FPE.SUB"

echo "building (drive C: -> $DRIVE) ..."
echo "----------------------------------------------------------------------"
printf 'SUBMIT FPE\n' | "$EMU" -d C=dir:"$DRIVE" 2>/dev/null \
	| LC_ALL=C tr -cd '\11\12\40-\176' | grep -v '^[[:space:]]*$' || true
echo "----------------------------------------------------------------------"

mkdir -p "$OUT"
status=0
for f in fpe.o fpedep.o; do
	U=$(printf '%s' "$f" | tr 'a-z' 'A-Z')
	if [ -s "$DRIVE/$U" ]; then
		cp "$DRIVE/$U" "$OUT/$f"
	else
		echo "error: $U was not produced" >&2
		status=1
	fi
done
[ "$status" -eq 0 ] && { echo "fpe library built into $OUT:"; ls -l "$OUT/fpe.o" "$OUT/fpedep.o"; }
exit "$status"
