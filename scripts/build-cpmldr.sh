#!/bin/sh
#
# build-cpmldr.sh -- build the CP/M-8000 loader (cpmldr.sys) from source using
# the emulator's in-guest DR/Zilog toolchain (zcc, asz8k, xcon, ld8k, ar8k).
#
# It stages the loader sources and the toolchain into a fresh temporary drive
# directory, mounts that directory as drive C: in the emulator, runs the
# distribution's own src/cpm8k/makeldr.sub, and copies the results
# (cpmldr.rel, cpmldr.sys) back out.
#
# Notes:
#   - makeldr.sub compiles bios.c with `-Dloader`. That DOES produce the loader
#     BIOS: the shipped zcc (v1.01e 12/26/84) honors -D, despite the older
#     12/19/84 src/cpm8k/readme claiming otherwise. See PROGRESS.md.
#   - The final `putboot cpmldr.sys a:` step in makeldr.sub writes a boot record
#     to a bootable drive A:, which this script does not mount; that step fails
#     harmlessly *after* cpmldr.sys has already been produced.
#   - The loader is built with the rebuilt-from-source ld8k in src/cpm8k, so it
#     is functional but not byte-identical to the distribution's cpmldr.sys
#     (see the loader notes in PROGRESS.md).
#
# Usage: scripts/build-cpmldr.sh [output-dir]      (default: build/ldr-src)

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

EMU=build/emu/cpm8k
SRC=${SRC:-src/cpm8k}
SUB=$SRC/makeldr.sub
OUT=${1:-build/ldr-src}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f build/bios-emu/cpm.sys ] || { echo "error: build/bios-emu/cpm.sys missing -- run 'make bios-emu' first" >&2; exit 1; }

# Sources makeldr.sub needs. lbiosasm.8kn (LOADER .equ 1) pulls in the same
# Loader BIOS sources (stock, from SRC). lbiosasm.8kn (LOADER .equ 1) .inputs
# the same .8kn set as the normal BIOS, so those must be present too.
BIOS_FILES="bios.c lbiosasm.8kn \
            biosdefs.8kn biosboot.8kn biosif.8kn biosio.8kn \
            biosmem.8kn biostrap.8kn syscall.8kn"

# BIOS-independent substrate (from SRC): ldrbdos.rel (loader BDOS), the
# assembler predef, and libcpm.a.
SUBSTRATE="ldrbdos.rel asz8k.pd libcpm.a"

# In-guest toolchain. asz8k chains to xcon (.OBJ -> .o); zcc chains to
# zcc1/zcc2/zcc3. makeldr.sub also invokes ar8k, pip and putboot.
TOOLS="zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k \
       asz8k.z8k xcon.z8k ld8k.z8k ar8k.z8k pip.z8k putboot.z8k"

# Fresh temporary drive directory, removed on exit.
DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-cpmldr.XXXXXX")
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
cp "$SUB" "$DRIVE/MAKELDR.SUB"

echo "building (drive C: -> $DRIVE) ..."
echo "----------------------------------------------------------------------"
printf 'SUBMIT MAKELDR\n' | "$EMU" -d C=dir:"$DRIVE" 2>/dev/null \
	| LC_ALL=C tr -cd '\11\12\40-\176' | grep -v '^[[:space:]]*$' || true
echo "----------------------------------------------------------------------"

mkdir -p "$OUT"
status=0
for f in cpmldr.rel cpmldr.sys; do
	U=$(printf '%s' "$f" | tr 'a-z' 'A-Z')
	if [ -s "$DRIVE/$U" ]; then
		cp "$DRIVE/$U" "$OUT/$f"
	else
		echo "error: $U was not produced" >&2
		status=1
	fi
done

if [ "$status" -eq 0 ]; then
	echo "loader built into $OUT:"
	ls -l "$OUT/cpmldr.rel" "$OUT/cpmldr.sys"
fi
exit "$status"
