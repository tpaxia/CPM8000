#!/bin/sh
# Compile, link, and execute the floating-point regression in hosted CP/M.

set -eu
export LC_ALL=C

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

MODEL=${1:-z8001}
case "$MODEL" in
	z8001|z8002) ;;
	*) echo "usage: $0 [z8001|z8002]" >&2; exit 2 ;;
esac

EMU=build/emu/cpm8k-$MODEL
SRC=src/cpm8k
OUT=build/fpe-regression/$MODEL

[ -x "$EMU" ] || { echo "error: $EMU missing -- run 'make emu'" >&2; exit 1; }
[ -s "build/bios-emu-$MODEL/cpm.sys" ] || {
	echo "error: build/bios-emu-$MODEL/cpm.sys missing -- run 'make emu'" >&2
	exit 1
}

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-fptest.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM
rm -rf "$OUT"
mkdir -p "$OUT"

cp tests/fptest.c "$DRIVE/FPTEST.C"
for file in zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k ld8k.z8k startup.o libcpm.a; do
	cp "$SRC/$file" "$DRIVE/"
done

LOG=$OUT/fptest.log
printf 'ZCC -C -M1 FPTEST.C\nLD8K -W -S -O FPTEST.Z8K STARTUP.O FPTEST.O -LCPM\nFPTEST\nEXIT\n' |
	"$EMU" -d C=dir:"$DRIVE" > "$LOG" 2>&1

tr -cd '\11\12\40-\176' < "$LOG" > "$OUT/fptest.txt"
if grep -q 'FPTEST PASS' "$OUT/fptest.txt" &&
	! grep -q 'FPTEST FAIL\|Z8000 invalid opcode' "$OUT/fptest.txt"; then
	echo "FPE regression passed: $MODEL"
else
	echo "FPE regression failed: $MODEL" >&2
	cat "$OUT/fptest.txt" >&2
	exit 1
fi
