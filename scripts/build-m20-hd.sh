#!/bin/sh
# Build a native M20 CP/M-8000 development hard disk for MAME drive C:.

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

OUT=${1:-build/m20-hd}
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-m20hd.XXXXXX")
WORK=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-m20hd-fs.XXXXXX")
trap 'rm -rf "$STAGE" "$WORK"' EXIT INT TERM

mkdir -p "$OUT"

# Begin with the complete distribution tree, but install the drive-C submit
# files below in place of the original floppy-oriented recipes.
for file in src/cpm8k/*; do
	case "$file" in
		*.sub) ;;
		*) cp "$file" "$STAGE/" ;;
	esac
done

# Add the assembler and linker sources used by the hosted build pipelines.
# Identically named headers are compatible; the maintained linker copies win.
for file in src/asm8k/*.c src/asm8k/*.h; do cp "$file" "$STAGE/"; done
for name in ld8k.c xout.h stdio.h portab.h; do
	cp "src/linker/$name" "$STAGE/"
done

# The games shipped with floppy-qualified include names.  The hard-disk
# workspace is self-contained, so make those includes use the current drive.
sed 's/"b:/"/g' src/cpm8k/wump.c > "$STAGE/WUMP.C"
sed 's/"b:/"/g' src/cpm8k/tictac.c > "$STAGE/TICTAC.C"

# putboot.c has a stale osif.h include although its own edit note says that
# cpm.h and bdos.h replaced it.  The complete bdos.h comes from src/asm8k.
sed -e 's/#include "osif.h"/#include "cpm.h"/' \
	-e 's/#include "xout.h"/#include "pbootx.h"/' \
	src/cpm8k/putboot.c > "$STAGE/PUTBOOT.C"
cp scripts/m20hd-pbootx.h "$STAGE/PBOOTX.H"

# CP/M has an 8.3 namespace.  These are the nine independent build recipes.
cp scripts/asz8k.sub      "$STAGE/ASZ8K.SUB"
cp scripts/ld8k.sub       "$STAGE/LD8K.SUB"
cp scripts/bios.sub       "$STAGE/BIOS.SUB"
cp scripts/cpmsys.sub     "$STAGE/CPMSYS.SUB"
cp scripts/linkcpmsys.sub "$STAGE/LINKSYS.SUB"
cp scripts/makeldr.sub    "$STAGE/MAKELDR.SUB"
cp scripts/mkputbt.sub    "$STAGE/MKPUTBT.SUB"
cp scripts/wump.sub       "$STAGE/WUMP.SUB"
cp scripts/tictac.sub     "$STAGE/TICTAC.SUB"

# cpmtools first looks for a file named diskdefs in its working directory.
cp src/diskdefs_m20_hd "$WORK/diskdefs"
LOGICAL="$WORK/m20hd.logical.img"
(
	cd "$WORK"
	mkfs.cpm -f m20hd "$LOGICAL"
	for file in "$STAGE"/*; do
		case "$file" in
			*.[cChHsS]|*.[sS][uU][bB]|*.[aA][sS][mM]|*.[pP][dD]|*.[8][kK][nN]|*/[rR][eE][aA][dD][mM][eE])
				# cpmcp's text mode supplies CR/LF conversion and the CP/M
				# 0x1a end marker.  Preserve distribution files that already
				# carry their original CP/M text padding byte-for-byte.
				last_byte=$(tail -c 1 "$file" | od -An -tu1 | tr -d ' ')
				if [ "$last_byte" = 26 ]; then
					cpmcp -f m20hd "$LOGICAL" "$file" 0:
				else
					cpmcp -f m20hd -t "$LOGICAL" "$file" 0:
				fi
				;;
			*) cpmcp -f m20hd "$LOGICAL" "$file" 0: ;;
		esac
	done
	fsck.cpm -f m20hd "$LOGICAL"
	cpmls -f m20hd -D "$LOGICAL"
)

RAW="$OUT/m20-cpm8000.raw"
CHD="$OUT/m20-cpm8000.chd"
python3 scripts/m20hd_pack.py "$LOGICAL" "$RAW"
# MAME's writable hard-disk image path requires an uncompressed CHD here.
# With a compressed CHD it can fall back to interpreting the container bytes
# as a raw disk, losing the embedded geometry and making CP/M drive C vanish.
chdman createhd -f -c none -i "$RAW" -o "$CHD" -chs 180,6,33 -ss 256

echo "Created M20 drive-C images:"
ls -lh "$RAW" "$CHD"
