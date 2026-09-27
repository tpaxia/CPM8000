#!/bin/sh
#
# build-bios.sh -- build the CP/M-8000 BIOS from source using the emulator's
# in-guest DR/Zilog toolchain (zcc, asz8k, xcon, ld8k, ar8k).
#
# It stages the BIOS sources and the toolchain into a fresh temporary drive
# directory, mounts that directory as drive C: in the emulator, runs
# scripts/bios.sub, and copies the results (bios.rel, bios.a) back out.
#
# TARGET_CPU (z8001|z8002, default z8001) selects the target-specific FPE
# objects linked into bios.rel.  HOST_CPU (default: TARGET_CPU) selects the
# hosted emulator that runs the toolchain; the result does not depend on it.
#
# Usage: [TARGET_CPU=...] [HOST_CPU=...] scripts/build-bios.sh [output-dir]
#        (default output-dir: build/bios-src)

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

TARGET_CPU=${TARGET_CPU:-z8001}
HOST_CPU=${HOST_CPU:-$TARGET_CPU}
case "$HOST_CPU" in
z8001|z8002) ;;
*) echo "error: unsupported HOST_CPU '$HOST_CPU'" >&2; exit 2 ;;
esac
EMU=build/emu/cpm8k-$HOST_CPU
SRC=${SRC:-src/cpm8k}
SUB=scripts/bios.sub
OUT=${1:-build/bios-src}

[ -x "$EMU" ] || { echo "error: $EMU not built -- run 'make emu' first" >&2; exit 1; }
[ -f "build/bios-emu-$HOST_CPU/cpm.sys" ] || {
	echo "error: build/bios-emu-$HOST_CPU/cpm.sys missing -- run 'make bios-emu-$HOST_CPU' first" >&2
	exit 1
}

# Sources bios.sub needs. biosasm.8kn pulls in the other .8kn files via
# `.input`, so they must be present too. Target-specific FPE objects are used
# from src/fpe/objects; scripts/regenerate-fpe-objects.sh rebuilds them from the
# maintained sources. asz8k.pd is the predef.
SOURCES="bios.c \
         biosasm.8kn biosdefs.8kn biosboot.8kn biosif.8kn biosio.8kn \
         biosmem.8kn biostrap.8kn syscall.8kn \
         asz8k.pd"

# In-guest toolchain. asz8k chains to xcon (.OBJ -> .o); zcc chains to
# zcc1/zcc2/zcc3.
TOOLS="zcc.z8k zcc1.z8k zcc2.z8k zcc3.z8k \
       asz8k.z8k xcon.z8k ld8k.z8k ar8k.z8k libcpm.a"

# Fresh temporary drive directory, removed on exit.
DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-bios.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM

echo "staging build inputs into temp drive: $DRIVE"
for f in $SOURCES $TOOLS; do cp "$SRC/$f" "$DRIVE/"; done

case "$TARGET_CPU" in
z8001)
	cp "src/fpe/objects/z8001/fpe.o" "$DRIVE/fpe.o"
	cp "src/fpe/objects/z8001/fpedep.o" "$DRIVE/fpedep.o"
	;;
z8002)
	cp "src/fpe/objects/z8002/fpe.o" "$DRIVE/fpe.o"
	cp "src/fpe/objects/z8002/fpedep.o" "$DRIVE/fpedep.o"
	;;
*)
	echo "error: unsupported TARGET_CPU '$TARGET_CPU'" >&2
	exit 2
	;;
esac

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
