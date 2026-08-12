#!/bin/sh
# Stage the common CP/M-8000 development tree for a logical media builder.

set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DEST=${1:?usage: stage-development.sh <destination> [bios-overlay]}
BIOS_OVERLAY=${2:-}

[ -d "$DEST" ] || { echo "error: destination '$DEST' does not exist" >&2; exit 1; }

# Begin with the regenerated distribution tree.  The media recipes below
# replace the floppy-oriented submits with self-contained current-drive ones.
for file in "$ROOT"/src/cpm8k/*; do
	case "$file" in
		*.[sS][uU][bB]) ;;
		*) cp "$file" "$DEST/" ;;
	esac
done

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
