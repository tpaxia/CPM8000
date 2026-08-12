#!/bin/sh
# Build the ten canonical submit pipelines on one fresh host-backed CP/M drive
# and compare their final artifacts with the checked-in SHA-256 baseline.

set -eu
export LC_ALL=C

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

MODEL=z8001
RECORD=0
while [ $# -gt 0 ]; do
	case "$1" in
		z8001|z8002) MODEL=$1 ;;
		--record) RECORD=1 ;;
		*) echo "usage: $0 [z8001|z8002] [--record]" >&2; exit 2 ;;
	esac
	shift
done

EMU=${CPM8K_EMU:-build/emu/cpm8k-$MODEL}
EMU_MODEL_ARG=
[ -z "${CPM8K_EMU:-}" ] || EMU_MODEL_ARG="-M $MODEL"
if [ "$RECORD" -eq 1 ]; then
	[ "$MODEL" = z8001 ] || {
		echo "error: the baseline must be recorded with Z8001" >&2
		exit 2
	}
	[ -n "${CPM8K_EMU:-}" ] || {
		echo "error: --record requires CPM8K_EMU=<known-good-emulator>" >&2
		exit 2
	}
fi
BASELINE=tests/submit-regression.sha256
OUT=build/submit-regression/$MODEL
[ -x "$EMU" ] || { echo "error: $EMU missing -- run 'make emu'" >&2; exit 1; }
[ -s "build/bios-emu-$MODEL/cpm.sys" ] || {
	echo "error: build/bios-emu-$MODEL/cpm.sys missing -- run 'make emu'" >&2
	exit 1
}

DRIVE=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-submit.XXXXXX")
trap 'rm -rf "$DRIVE"' EXIT INT TERM
rm -rf "$OUT"
mkdir -p "$OUT"

scripts/media/stage-development.sh "$DRIVE"

run_submit()
{
	name=$1
	echo "-- SUBMIT $name ($MODEL) --"
	log=$OUT/$name.log
	# EMU_MODEL_ARG is used only with an explicitly supplied legacy executable.
	# shellcheck disable=SC2086
	printf 'SUBMIT %s\n' "$name" | "$EMU" $EMU_MODEL_ARG -d C=dir:"$DRIVE" > "$log" 2>&1
	LC_ALL=C tr -cd '\11\12\40-\176' < "$log" \
		| grep -E 'error|fatal|Hosted CPU|CP/M-8000|C>' || true
	if LC_ALL=C tr -cd '\11\12\40-\176' < "$log" \
		| grep -Eiq '(^|[^0-9])(fatal error|[1-9][0-9]* errors?|disk select error|unexpected EOF|no input file|not found)'; then
		echo "error: SUBMIT $name reported a failure (see $log)" >&2
		exit 1
	fi
}

copy_result()
{
	source=$1
	dest=$2
	[ -s "$DRIVE/$source" ] || { echo "error: $source was not produced" >&2; exit 1; }
	cp "$DRIVE/$source" "$OUT/$dest"
}

rm -f "$DRIVE/_ASZ8K.Z8K"
run_submit ASZ8K
copy_result _ASZ8K.Z8K asz8k.z8k
rm -f "$DRIVE/_LD8K.Z8K"
run_submit LD8K
copy_result _LD8K.Z8K ld8k.z8k
rm -f "$DRIVE/FPE.O" "$DRIVE/FPEDEP.O"
run_submit FPE
copy_result FPE.O fpe.o
copy_result FPEDEP.O fpedep.o
rm -f "$DRIVE/BIOS.REL" "$DRIVE/BIOS.A"
run_submit BIOS
copy_result BIOS.REL bios.rel
copy_result BIOS.A bios.a
rm -f "$DRIVE/CPM.SYS"
run_submit CPMSYS
copy_result CPM.SYS cpmsys-cpm.sys
rm -f "$DRIVE/CPM.SYS"
run_submit LINKSYS
copy_result CPM.SYS linksys-cpm.sys
rm -f "$DRIVE/CPMLDR.REL" "$DRIVE/CPMLDR.SYS"
run_submit MAKELDR
copy_result CPMLDR.REL cpmldr.rel
copy_result CPMLDR.SYS cpmldr.sys
rm -f "$DRIVE/PUTBOOT.Z8K"
run_submit MKPUTBT
copy_result PUTBOOT.Z8K putboot.z8k
rm -f "$DRIVE/WUMP.Z8K"
run_submit WUMP
copy_result WUMP.Z8K wump.z8k
rm -f "$DRIVE/TICTAC.Z8K"
run_submit TICTAC
copy_result TICTAC.Z8K tictac.z8k

hash_one()
{
	if command -v sha256sum >/dev/null 2>&1; then
		sha256sum "$1" | awk '{print $1}'
	else
		shasum -a 256 "$1" | awk '{print $1}'
	fi
}

ACTUAL=$OUT/SHA256SUMS
: > "$ACTUAL"
for file in "$OUT"/*; do
	case "$file" in *.log|*/SHA256SUMS) continue ;; esac
	printf '%s  %s\n' "$(hash_one "$file")" "$(basename "$file")" >> "$ACTUAL"
done

if [ "$RECORD" -eq 1 ]; then
	cp "$ACTUAL" "$BASELINE"
	echo "recorded baseline: $BASELINE"
elif cmp -s "$BASELINE" "$ACTUAL"; then
	echo "submit regression passed: $MODEL"
else
	echo "submit regression failed: $MODEL" >&2
	diff -u "$BASELINE" "$ACTUAL" || true
	exit 1
fi
