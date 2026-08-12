#!/bin/sh
# Stage the common CP/M-8000 development tree for a logical media builder.

set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DEST=${1:?usage: stage-development.sh <destination> [bios-overlay] [cpu-model]}
BIOS_OVERLAY=${2:-}
CPU_MODEL=${3:-}

if [ -z "$CPU_MODEL" ] && [ -n "$BIOS_OVERLAY" ] && [ -f "$BIOS_OVERLAY/EMU_MODEL" ]; then
	CPU_MODEL=$(sed -n '1p' "$BIOS_OVERLAY/EMU_MODEL")
fi
: "${CPU_MODEL:=z8001}"
case "$CPU_MODEL" in
	z8001|z8002) ;;
	*) echo "error: unsupported CPU model '$CPU_MODEL'" >&2; exit 2 ;;
esac

[ -d "$DEST" ] || { echo "error: destination '$DEST' does not exist" >&2; exit 1; }

# Begin with the regenerated distribution tree.  The media recipes below
# replace the floppy-oriented submits with self-contained current-drive ones.
for file in "$ROOT"/src/cpm8k/*; do
	case "$file" in
		*.[sS][uU][bB]) ;;
		*) cp "$file" "$DEST/" ;;
	esac
done

# Always replace the regenerated distribution copies with the authoritative
# maintained FPE sources and the definitions for the selected CPU.
cp "$ROOT/src/fpe/fpe.z8k" "$DEST/FPE.8KN"
case "$CPU_MODEL" in
z8001)
	cp "$ROOT/src/fpe/fpedep.z8k" "$DEST/FPEDEP.8KN"
	cp "$ROOT/src/fpe/biosdefs.z8k" "$DEST/BIOSDEFS.Z8K"
	;;
z8002)
	cp "$ROOT/src/fpe/fpedep-z8002.z8k" "$DEST/FPEDEP.8KN"
	cp "$ROOT/src/fpe/biosdefs-z8002.z8k" "$DEST/BIOSDEFS.Z8K"
	;;
esac

# Common assembler and linker sources.  These tools are CP/M-8000 software,
# not M20 BIOS components.
for file in "$ROOT"/src/asm8k/*.c "$ROOT"/src/asm8k/*.h; do
	cp "$file" "$DEST/"
done
for name in ld8k.c xout.h stdio.h portab.h; do
	cp "$ROOT/src/linker/$name" "$DEST/"
done

# Self-contained development-media variants of distribution sources.
sed 's/"b:/"/g' "$ROOT/src/cpm8k/wump.c" > "$DEST/WUMP.C"
sed 's/"b:/"/g' "$ROOT/src/cpm8k/tictac.c" > "$DEST/TICTAC.C"
sed -e 's/#include "osif.h"/#include "cpm.h"/' \
	-e 's/#include "xout.h"/#include "pbootx.h"/' \
	"$ROOT/src/cpm8k/putboot.c" > "$DEST/PUTBOOT.C"
cp "$ROOT/scripts/media/pbootx.h" "$DEST/PBOOTX.H"

for name in asz8k ld8k fpe bios cpmsys makeldr mkputbt wump tictac; do
	upper=$(printf '%s' "$name" | tr 'a-z' 'A-Z')
	cp "$ROOT/scripts/$name.sub" "$DEST/$upper.SUB"
done
cp "$ROOT/scripts/linkcpmsys.sub" "$DEST/LINKSYS.SUB"

# A target package may replace or add BIOS source files and target-specific
# submit recipes.  It does not own the compiler, assembler, linker, or the
# other common development content.
if [ -n "$BIOS_OVERLAY" ]; then
	for file in "$BIOS_OVERLAY"/*.c "$BIOS_OVERLAY"/*.8kn "$BIOS_OVERLAY"/*.sub; do
		if [ -f "$file" ]; then
			base=$(basename "$file" | tr 'a-z' 'A-Z')
			cp "$file" "$DEST/$base"
		fi
	done
fi

# Z8002-demo uses its monitor boot payload and ATA image builder. The M20
# loader and putboot recipes are not applicable to that target.
if [ "$CPU_MODEL" = z8002 ]; then
	rm -f "$DEST/MAKELDR.SUB" "$DEST/MKPUTBT.SUB"
fi
