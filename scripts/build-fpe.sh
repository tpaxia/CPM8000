#!/bin/sh
#
# build-fpe.sh -- assemble the CP/M-8000 floating-point library (fpe.o,
# fpedep.o) from source (src/fpe) using the emulator's in-guest assembler.
#
# fpe.z8k is the shared Z8000 EPA extended-instruction trap handler. Each target
# supplies its own saved-frame definitions and memory helper. The sources are
# assembled with the distribution asz8k, whose V1.1B syntax supports the FP
# instructions. ASZ8K chains to XCON as its implicit second pass.
#
# NOTE on reproduction vs the distribution objects:
#  - fpe.o has equivalent executable content. Its epuwp work area is declared
#    with .block (reserved, uninitialized), so this build emits clean zeros
#    where the distribution object contains leftover buffer data.
#  - fpedep.o has the distribution object's machine code and relocations; its
#    function bodies were transcribed from that object's disassembly.
#  - Both maintained objects additionally expose the co and pcbase parameter
#    symbols used to select the target call and saved-PC frame layout.
#
# Usage: scripts/build-fpe.sh [z8001|z8002] [output-dir]

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU_MODEL=${1:-z8001}
case "$EMU_MODEL" in
	z8001) BIOSDEFS=biosdefs.z8k; FPEDEP=fpedep.z8k ;;
	z8002) BIOSDEFS=biosdefs-z8002.z8k; FPEDEP=fpedep-z8002.z8k ;;
	*) echo "usage: $0 [z8001|z8002] [output-dir]" >&2; exit 2 ;;
esac
BUILD_EMU_MODEL=${BUILD_EMU_MODEL:-$EMU_MODEL}
EMU=build/emu/cpm8k-$BUILD_EMU_MODEL
SRC=src/cpm8k
FSRC=src/fpe
SUB=scripts/fpe.sub
OUT=${2:-build/fpe-$EMU_MODEL}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f build/bios-emu-$BUILD_EMU_MODEL/cpm.sys ] || {
	echo "error: build/bios-emu-$BUILD_EMU_MODEL/cpm.sys missing -- run 'make bios-emu-$BUILD_EMU_MODEL' first" >&2
	exit 1
}

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-fpe.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
# main sources -> .8kn (asz8k requires a .8k{n,s} main file); includes stay .z8k
cp "$FSRC/fpe.z8k"    "$DRIVE/fpe.8kn"
cp "$FSRC/$FPEDEP" "$DRIVE/fpedep.8kn"
cp "$FSRC/$BIOSDEFS" "$DRIVE/biosdefs.z8k"
# assembler + converter + predef (distribution asz8k supports the FP ops)
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
