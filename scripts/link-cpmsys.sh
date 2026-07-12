#!/bin/sh
#
# link-cpmsys.sh -- final system link.
#
# Link a prebuilt BIOS object (bios.rel, from a BIOS package's Makefile) with
# the CCP+BDOS (cpmsys.rel) and C library (libcpm.a) into cpm.sys, using ld8k
# inside the emulator. The BIOS object and the system substrate are linked
# separately: the BIOS package builds bios.rel; this does the final system link.
#
# Usage: scripts/link-cpmsys.sh <bios.rel> <out-dir>   -> <out-dir>/cpm.sys

set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU=build/emu/cpm8k
SRC=src/cpm8k
SUB=scripts/linkcpmsys.sub
BIOS_REL=${1:?usage: link-cpmsys.sh <bios.rel> <out-dir>}
OUT=${2:?usage: link-cpmsys.sh <bios.rel> <out-dir>}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -s "$BIOS_REL" ] || { echo "error: BIOS object '$BIOS_REL' missing" >&2; exit 1; }

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-linksys.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

cp "$BIOS_REL" "$DRIVE/bios.rel"
for f in cpmsys.rel libcpm.a ld8k.z8k; do cp "$SRC/$f" "$DRIVE/"; done
cp "$SUB" "$DRIVE/LINKSYS.SUB"

printf 'SUBMIT LINKSYS\n' | "$EMU" -d C=dir:"$DRIVE" 2>/dev/null \
	| LC_ALL=C tr -cd '\11\12\40-\176' | grep -v '^[[:space:]]*$' || true

mkdir -p "$OUT"
[ -s "$DRIVE/CPM.SYS" ] || { echo "error: CPM.SYS was not produced" >&2; exit 1; }
cp "$DRIVE/CPM.SYS" "$OUT/cpm.sys"
