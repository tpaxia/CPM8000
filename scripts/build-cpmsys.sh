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

# Stock BIOS sources (from SRC). biosasm.8kn .inputs the other .8kn files.
BIOS_FILES="bios.c \
            biosasm.8kn biosdefs.8kn biosboot.8kn biosif.8kn biosio.8kn \
            biosmem.8kn biostrap.8kn syscall.8kn"

# BIOS-independent substrate (from SRC): prebuilt fpe.o/fpedep.o, the assembler
# predef, the CCP+BDOS (cpmsys.rel) and C library (libcpm.a, linked via -lcpm).
SUBSTRATE="fpe.o fpedep.o asz8k.pd cpmsys.rel libcpm.a"

# In-guest toolchain. asz8k chains to xcon; zcc chains to zcc1/2/3.
TOOLS="zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k \
       asz8k.z8k xcon.z8k ld8k.z8k"

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-cpmsys.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
for f in $BIOS_FILES $SUBSTRATE $TOOLS; do cp "$SRC/$f" "$DRIVE/"; done

# Optional BIOS overlay: a package dir (src/bios/<name>) supplies BIOS-specific
# sources (.c/.8kn) overriding/adding to the stock set. Empty for stock M20.
if [ -n "${BIOS_OVERLAY:-}" ]; then
	echo "overlaying BIOS sources from: $BIOS_OVERLAY"
	for f in "$BIOS_OVERLAY"/*.8kn "$BIOS_OVERLAY"/*.c; do
		[ -f "$f" ] && cp "$f" "$DRIVE/"
	done
fi
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
