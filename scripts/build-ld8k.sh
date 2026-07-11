#!/bin/sh
#
# build-ld8k.sh -- build the ld8k linker from source (src/linker/ld8k.c) using
# the emulator's in-guest DR/Zilog toolchain.
#
# This is a bootstrap build: it uses the existing ld8k.z8k to compile and link
# a new one. It stages the linker source, headers, startup.o, libcpm.a and the
# toolchain into a fresh temporary drive mounted as C:, runs scripts/ld8k.sub,
# and copies the result out. The recipe writes _ld8k.z8k (leading underscore)
# so it never overwrites the running linker mid-build.
#
# Usage: scripts/build-ld8k.sh [output-dir]      (default: build/linker)

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU=build/emu/cpm8k
SRC=src/cpm8k
LSRC=src/linker
SUB=scripts/ld8k.sub
OUT=${1:-build/linker}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f build/bios-emu/cpm.sys ] || { echo "error: build/bios-emu/cpm.sys missing -- run 'make bios-emu' first" >&2; exit 1; }

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-ld8k.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
# linker source + headers
for f in ld8k.c xout.h stdio.h portab.h; do cp "$LSRC/$f" "$DRIVE/"; done
# startup object and C library
cp "$SRC/startup.o" "$DRIVE/"
cp "$SRC/libcpm.a"  "$DRIVE/"
# toolchain: zcc (chains zcc1/2/3) and the bootstrap ld8k linker
for f in zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k ld8k.z8k; do cp "$SRC/$f" "$DRIVE/"; done
cp "$SUB" "$DRIVE/LD8K.SUB"

echo "building (drive C: -> $DRIVE) ..."
echo "----------------------------------------------------------------------"
printf 'SUBMIT LD8K\n' | "$EMU" -d C=dir:"$DRIVE" 2>/dev/null \
	| LC_ALL=C tr -cd '\11\12\40-\176' | grep -v '^[[:space:]]*$' || true
echo "----------------------------------------------------------------------"

mkdir -p "$OUT"
if [ -s "$DRIVE/_LD8K.Z8K" ]; then
	cp "$DRIVE/_LD8K.Z8K" "$OUT/ld8k.z8k"
	echo "ld8k built into $OUT:"
	ls -l "$OUT/ld8k.z8k"
else
	echo "error: _LD8K.Z8K was not produced" >&2
	exit 1
fi
