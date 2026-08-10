#!/bin/sh
# Pack a staged flat CP/M 8.3 tree into one logical image or a floppy set.

set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
STAGE=${1:?usage: pack-cpm.sh <stage> <format.conf> <output-dir>}
CONFIG=${2:?usage: pack-cpm.sh <stage> <format.conf> <output-dir>}
OUT=${3:?usage: pack-cpm.sh <stage> <format.conf> <output-dir>}

# Trusted repository format descriptor.  It defines CPM_FORMAT, DISKDEFS,
# LAYOUT (single or split), IMAGE_BASENAME, and IMAGE_SIZE.
. "$CONFIG"
: "${CPM_FORMAT:?missing CPM_FORMAT in $CONFIG}"
: "${DISKDEFS:?missing DISKDEFS in $CONFIG}"
: "${LAYOUT:?missing LAYOUT in $CONFIG}"
: "${IMAGE_BASENAME:?missing IMAGE_BASENAME in $CONFIG}"
: "${IMAGE_SIZE:?missing IMAGE_SIZE in $CONFIG}"

WORK=$(mktemp -d "${TMPDIR:-/tmp}/cpm8k-media-pack.XXXXXX")
trap 'rm -rf "$WORK"' EXIT INT TERM
cp "$ROOT/$DISKDEFS" "$WORK/diskdefs"
mkdir -p "$OUT"
find "$OUT" -maxdepth 1 -type f -name "$IMAGE_BASENAME*.img" -delete

copy_file()
{
	copy_image=$1
	copy_source=$2
	copy_base=$(basename "$copy_source" | tr 'a-z' 'A-Z')
	copy_text=0
	case "$copy_base" in
		BIOSDEFS.Z8K|README|*.[CHS]|*.SUB|*.ASM|*.PD|*.8KN) copy_text=1 ;;
	esac
	if [ "$copy_text" -eq 1 ] && [ "$(tail -c 1 "$copy_source" | od -An -tu1 | tr -d ' ')" != 26 ]; then
		(cd "$WORK" && cpmcp -f "$CPM_FORMAT" -t "$copy_image" "$copy_source" 0:)
	else
		(cd "$WORK" && cpmcp -f "$CPM_FORMAT" "$copy_image" "$copy_source" 0:)
	fi
}

new_image()
{
	new_path=$1
	rm -f "$new_path"
	(
		cd "$WORK"
		mkfs.cpm -f "$CPM_FORMAT" "$new_path"
	)
	truncate -s "$IMAGE_SIZE" "$new_path"
}

check_image()
{
	check_path=$1
	(
		cd "$WORK"
		fsck.cpm -f "$CPM_FORMAT" "$check_path"
	)
}

FILES="$WORK/files"
find "$STAGE" -maxdepth 1 -type f | LC_ALL=C sort > "$FILES"

case "$LAYOUT" in
single)
	image="$WORK/$IMAGE_BASENAME.img"
	new_image "$image"
	while IFS= read -r file; do copy_file "$image" "$file"; done < "$FILES"
	check_image "$image"
	cp "$image" "$OUT/$IMAGE_BASENAME.img"
	;;
split)
	number=1
	count=0
	image="$WORK/current.img"
	new_image "$image"
	while IFS= read -r file; do
		candidate="$WORK/candidate.img"
		cp "$image" "$candidate"
		if copy_file "$candidate" "$file" >/dev/null 2>&1; then
			mv "$candidate" "$image"
			count=$((count + 1))
			continue
		fi
		rm -f "$candidate"
		[ "$count" -gt 0 ] || { echo "error: $(basename "$file") does not fit on an empty $CPM_FORMAT image" >&2; exit 1; }
		check_image "$image"
		suffix=$(printf '%02d' "$number")
		cp "$image" "$OUT/$IMAGE_BASENAME-$suffix.img"
		number=$((number + 1))
		count=0
		new_image "$image"
		copy_file "$image" "$file"
		count=1
	done < "$FILES"
	if [ "$count" -gt 0 ]; then
		check_image "$image"
		suffix=$(printf '%02d' "$number")
		cp "$image" "$OUT/$IMAGE_BASENAME-$suffix.img"
	fi
	;;
*)
	echo "error: unsupported media layout '$LAYOUT'" >&2
	exit 2
	;;
esac

echo "logical CP/M media created in $OUT:"
ls -lh "$OUT"/*.img
