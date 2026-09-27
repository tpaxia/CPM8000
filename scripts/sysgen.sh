#!/bin/sh
#
# sysgen.sh -- generate CP/M-8000 guest system binaries for a chosen BIOS.
#
# A BIOS is a directory with a Makefile (a "BIOS package") under src/bios/. It
# builds a BIOS object with `make bios.rel`; sysgen then does the final system
# link (the package builds the object, sysgen links the system). The BIOS is
# built with the in-emulator DR/Zilog toolchain (x.out objects). Output goes to
# build/system/<name>/.
#
# The target CPU is a property of the package (its TARGET_CPU file, default
# z8001).  The host -- the hosted emulator that runs the guest toolchain -- is
# chosen per build and defaults to the target; either host produces identical
# binaries.
#
# Usage:
#   scripts/sysgen.sh [--bios DIR] [--host z8001|z8002] [--loader] <name>
#
#     --bios DIR   BIOS package directory. Default: src/bios/<name>.
#     --host CPU   hosted CPU that runs the build.  Default: $HOST_CPU, else the
#                  package's target CPU.
#     --loader     also build the cold-boot loader cpmldr.sys.  Installing it
#                  with putboot is a separate, target-specific operation.
#     <name>       system name; artifacts land in build/system/<name>/.

set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

BIOSDIR=""
LOADER=0
NAME=""
HOST_CPU=${HOST_CPU:-}
while [ $# -gt 0 ]; do
	case "$1" in
		--bios)    BIOSDIR=$2; shift 2 ;;
		--host)    HOST_CPU=$2; shift 2 ;;
		--loader)  LOADER=1; shift ;;
		-h|--help) sed -n '2,24p' "$0"; exit 0 ;;
		-*)        echo "unknown option: $1" >&2; exit 2 ;;
		*)         NAME=$1; shift ;;
	esac
done
[ -n "$NAME" ] || { echo "usage: scripts/sysgen.sh [--bios DIR] [--host CPU] [--loader] <name>" >&2; exit 2; }
: "${BIOSDIR:=src/bios/$NAME}"
[ -f "$BIOSDIR/Makefile" ] || { echo "error: '$BIOSDIR' is not a BIOS package (no Makefile)" >&2; exit 2; }

TARGET_CPU=z8001
[ -f "$BIOSDIR/TARGET_CPU" ] && TARGET_CPU=$(sed -n '1p' "$BIOSDIR/TARGET_CPU")
: "${HOST_CPU:=$TARGET_CPU}"
for cpu in "$TARGET_CPU" "$HOST_CPU"; do
	case "$cpu" in
		z8001|z8002) ;;
		*) echo "error: unsupported CPU '$cpu' (z8001 or z8002)" >&2; exit 2 ;;
	esac
done
CPMSYS=cpmsys.rel
[ -f "$BIOSDIR/CPMSYS" ] && CPMSYS=$(sed -n '1p' "$BIOSDIR/CPMSYS")

OBJ=$ROOT/build/bios/$NAME       # intermediate BIOS object
OUT=$ROOT/build/system/$NAME     # final system artifacts
echo "===================================================================="
echo " sysgen: system '$NAME'   BIOS: $BIOSDIR   target: $TARGET_CPU   host: $HOST_CPU"
echo "===================================================================="
mkdir -p "$OBJ" "$OUT"

echo "-- building BIOS object (make -C $BIOSDIR bios.rel) --"
make -C "$BIOSDIR" bios.rel BUILDDIR="$OBJ" HOST_CPU="$HOST_CPU" >/dev/null
[ -s "$OBJ/bios.rel" ] || { echo "error: $BIOSDIR did not produce bios.rel" >&2; exit 1; }

echo "-- linking cpm.sys --"
./scripts/link-cpmsys.sh "$OBJ/bios.rel" "$OUT" "$CPMSYS" "$HOST_CPU" >/dev/null
[ -s "$OUT/cpm.sys" ] || { echo "error: cpm.sys was not produced" >&2; exit 1; }

if make -s -C "$BIOSDIR" -n system-artifacts >/dev/null 2>&1; then
	echo "-- building target system artifacts --"
	make -s -C "$BIOSDIR" system-artifacts SYSTEMDIR="$OUT"
fi

if [ "$LOADER" -eq 1 ]; then
	echo "-- building loader cpmldr.sys --"
	BIOS_OVERLAY="$ROOT/$BIOSDIR" HOST_CPU="$HOST_CPU" ./scripts/build-cpmldr.sh "$OUT" >/dev/null
	[ -s "$OUT/cpmldr.sys" ] || { echo "error: cpmldr.sys was not produced" >&2; exit 1; }
fi

echo "--------------------------------------------------------------------"
echo "system '$NAME' generated in $OUT:"
ls -l "$OUT"
