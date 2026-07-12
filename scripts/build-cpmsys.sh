#!/bin/sh
#
# build-cpmsys.sh -- build cpm.sys from source using the emulator's in-guest
# DR/Zilog toolchain.
#
# cpm.sys is the CCP + BDOS (shipped prebuilt as cpmsys.rel) linked with the
# BIOS. This script builds bios.rel from source (zcc, asz8k -> xcon, ld8k),
# then links cpm.sys = bios.rel + cpmsys.rel + libcpm.a, inside a fresh
# temporary drive mounted as C:, and copies the result out.
#
# Usage: scripts/build-cpmsys.sh [output-dir]    (default: build/bios-src)

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU=build/emu/cpm8k
SRC=${SRC:-src/cpm8k}
SUB=scripts/cpmsys.sub
OUT=${1:-build/bios-src}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f build/bios-emu/cpm.sys ] || { echo "error: build/bios-emu/cpm.sys missing -- run 'make bios-emu' first" >&2; exit 1; }

# BIOS sources (biosasm.8kn .inputs the other .8kn files), prebuilt
# fpe.o/fpedep.o, the assembler predef, plus cpmsys.rel (the CCP+BDOS) and
# libcpm.a (the C library, linked via -lcpm).
SOURCES="bios.c \
         biosasm.8kn biosdefs.8kn biosboot.8kn biosif.8kn biosio.8kn \
         biosmem.8kn biostrap.8kn syscall.8kn \
         fpe.o fpedep.o asz8k.pd \
         cpmsys.rel libcpm.a"

# In-guest toolchain. asz8k chains to xcon; zcc chains to zcc1/2/3.
TOOLS="zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k \
       asz8k.z8k xcon.z8k ld8k.z8k"

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-cpmsys.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
for f in $SOURCES $TOOLS; do cp "$SRC/$f" "$DRIVE/"; done
cp "$SUB" "$DRIVE/CPMSYS.SUB"

echo "building (drive C: -> $DRIVE) ..."
echo "----------------------------------------------------------------------"
printf 'SUBMIT CPMSYS\n' | "$EMU" -d C=dir:"$DRIVE" 2>/dev/null \
	| LC_ALL=C tr -cd '\11\12\40-\176' | grep -v '^[[:space:]]*$' || true
echo "----------------------------------------------------------------------"

mkdir -p "$OUT"
if [ -s "$DRIVE/CPM.SYS" ]; then
	cp "$DRIVE/CPM.SYS" "$OUT/cpm.sys"
	echo "cpm.sys built into $OUT:"
	ls -l "$OUT/cpm.sys"
else
	echo "error: CPM.SYS was not produced" >&2
	exit 1
fi
