#!/bin/sh
#
# build-asz8k.sh -- build the asz8k assembler from source (src/asm8k) using the
# emulator's in-guest DR/Zilog toolchain.
#
# NOTE: these sources are an earlier (pre-FPU) assembler version. They rebuild a
# working assembler that reproduces integer-only objects (e.g. biosasm.o /
# bios.rel) byte-for-byte, but they do NOT support the Z8070 floating-point
# instructions (fldctl/fldil/f0-f7) used by startup.8kn, so they cannot yet
# rebuild the full toolchain. The matching predef is src/asm8k/asz8k.pd (this
# is a different predef from the distribution's V1.1B src/cpm8k/asz8k.pd).
#
# It stages the sources and toolchain into a fresh temporary drive mounted as
# C:, runs scripts/asz8k.sub, and copies the result out. The recipe writes
# _asz8k.z8k (leading underscore) so it never clobbers a running assembler.
#
# Usage: scripts/build-asz8k.sh [output-dir]     (default: build/asm8k)

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU=build/emu/cpm8k
SRC=src/cpm8k
ASRC=src/asm8k
SUB=scripts/asz8k.sub
OUT=${1:-build/asm8k}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f build/bios-emu/cpm.sys ] || { echo "error: build/bios-emu/cpm.sys missing -- run 'make bios-emu' first" >&2; exit 1; }

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-asz8k.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
# assembler sources, headers, chain.c wrapper (everything in src/asm8k)
for f in "$ASRC"/*.c "$ASRC"/*.h; do cp "$f" "$DRIVE/"; done
# startup object and C library
cp "$SRC/startup.o" "$DRIVE/"
cp "$SRC/libcpm.a"  "$DRIVE/"
# toolchain: zcc (chains zcc1/2/3), ld8k, ar8k
for f in zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k ld8k.z8k ar8k.z8k; do cp "$SRC/$f" "$DRIVE/"; done
cp "$SUB" "$DRIVE/ASZ8K.SUB"

echo "building (drive C: -> $DRIVE) ..."
echo "----------------------------------------------------------------------"
printf 'SUBMIT ASZ8K\n' | "$EMU" -d C=dir:"$DRIVE" 2>/dev/null \
	| LC_ALL=C tr -cd '\11\12\40-\176' | grep -v '^[[:space:]]*$' || true
echo "----------------------------------------------------------------------"

mkdir -p "$OUT"
if [ -s "$DRIVE/_ASZ8K.Z8K" ]; then
	cp "$DRIVE/_ASZ8K.Z8K" "$OUT/asz8k.z8k"
	echo "asz8k built into $OUT:"
	ls -l "$OUT/asz8k.z8k"
else
	echo "error: _ASZ8K.Z8K was not produced" >&2
	exit 1
fi
