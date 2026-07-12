#!/bin/sh
#
# build-bios.sh -- build the CP/M-8000 BIOS from source using the emulator's
# in-guest DR/Zilog toolchain (zcc, asz8k, xcon, ld8k, ar8k).
#
# It stages the BIOS sources and the toolchain into a fresh temporary drive
# directory, mounts that directory as drive C: in the emulator, runs
# scripts/bios.sub, and copies the results (bios.rel, bios.a) back out.
#
# Usage: scripts/build-bios.sh [output-dir]      (default: build/bios-src)

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU=build/emu/cpm8k
SRC=${SRC:-src/cpm8k}
SUB=scripts/bios.sub
OUT=${1:-build/bios-src}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f build/bios-emu/cpm.sys ] || { echo "error: build/bios-emu/cpm.sys missing -- run 'make bios-emu' first" >&2; exit 1; }

# Sources bios.sub needs. biosasm.8kn pulls in the other .8kn files via
# `.input`, so they must be present too. fpe.o/fpedep.o are used prebuilt
# (their .8kn sources are not shipped). asz8k.pd is the assembler predef.
SOURCES="bios.c \
         biosasm.8kn biosdefs.8kn biosboot.8kn biosif.8kn biosio.8kn \
         biosmem.8kn biostrap.8kn syscall.8kn \
         fpe.o fpedep.o asz8k.pd"

# In-guest toolchain. asz8k chains to xcon (.OBJ -> .o); zcc chains to
# zcc1/zcc2/zcc3.
TOOLS="zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k \
       asz8k.z8k xcon.z8k ld8k.z8k ar8k.z8k libcpm.a"

# Fresh temporary drive directory, removed on exit.
DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-bios.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
for f in $SOURCES $TOOLS; do cp "$SRC/$f" "$DRIVE/"; done

# Optional BIOS overlay: a package dir (e.g. src/bios/<name>) supplies the
# BIOS-specific sources (.c/.8kn) that override or add to the stock M20 set --
# the same overlay idea as regenerate+overlay for src/cpm8k. For the stock M20
# BIOS the overlay is empty, so this is a no-op.
if [ -n "${BIOS_OVERLAY:-}" ]; then
	echo "overlaying BIOS sources from: $BIOS_OVERLAY"
	for f in "$BIOS_OVERLAY"/*.8kn "$BIOS_OVERLAY"/*.c; do
		[ -f "$f" ] && cp "$f" "$DRIVE/"
	done
fi
cp "$SUB" "$DRIVE/BIOS.SUB"

echo "building (drive C: -> $DRIVE) ..."
echo "----------------------------------------------------------------------"
printf 'SUBMIT BIOS\n' | "$EMU" -d C=dir:"$DRIVE" 2>/dev/null \
	| LC_ALL=C tr -cd '\11\12\40-\176' | grep -v '^[[:space:]]*$' || true
echo "----------------------------------------------------------------------"

mkdir -p "$OUT"
status=0
for f in bios.rel bios.a; do
	U=$(printf '%s' "$f" | tr 'a-z' 'A-Z')
	if [ -s "$DRIVE/$U" ]; then
		cp "$DRIVE/$U" "$OUT/$f"
	else
		echo "error: $U was not produced" >&2
		status=1
	fi
done

if [ "$status" -eq 0 ]; then
	echo "BIOS built into $OUT:"
	ls -l "$OUT/bios.rel" "$OUT/bios.a"
fi
exit "$status"
