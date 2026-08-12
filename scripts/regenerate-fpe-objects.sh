#!/bin/sh
# Rebuild the checked-in target-specific FPE x.out objects inside hosted CP/M.

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

MODE=replace
case "${1:-}" in
	"") ;;
	--verify) MODE=verify ;;
	*) echo "usage: $0 [--verify]" >&2; exit 2 ;;
esac

WORK=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-fpe-objects.XXXXXX")
trap 'rm -rf "$WORK"' EXIT INT TERM

for model in z8001 z8002; do
	scripts/build-fpe.sh "$model" "$WORK/$model"
	dest="src/fpe/objects/$model"
	if [ "$MODE" = verify ]; then
		cmp "$WORK/$model/fpe.o" "$dest/fpe.o"
		cmp "$WORK/$model/fpedep.o" "$dest/fpedep.o"
	else
		mkdir -p "$dest"
		cp "$WORK/$model/fpe.o" "$dest/fpe.o"
		cp "$WORK/$model/fpedep.o" "$dest/fpedep.o"
	fi
done

if [ "$MODE" = verify ]; then
	echo "checked-in Z8001 and Z8002 FPE objects reproduce exactly"
else
	echo "updated checked-in Z8001 and Z8002 FPE objects"
fi
