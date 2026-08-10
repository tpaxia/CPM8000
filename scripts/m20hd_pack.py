#!/usr/bin/env python3
"""Pack the M20 CP/M logical filesystem into its 180x6x33 CHS layout."""

import argparse
from pathlib import Path


CYLINDERS = 180
HEADS = 6
DATA_SECTORS = 32
PHYSICAL_SECTORS = 33
SECTOR_SIZE = 256


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("logical", type=Path)
    parser.add_argument("physical", type=Path)
    args = parser.parse_args()

    logical = bytearray(args.logical.read_bytes())
    capacity = CYLINDERS * HEADS * DATA_SECTORS * SECTOR_SIZE
    if len(logical) > capacity:
        raise SystemExit(f"logical image is {len(logical)} bytes; maximum is {capacity}")

    logical += bytes(capacity - len(logical))

    # The stock monitor reads the bad-block table from CHS 0/0/1.  It is in
    # CP/M's reserved area: zero entries followed by the required 0xff marker.
    logical[SECTOR_SIZE] = 0
    logical[SECTOR_SIZE + 1] = 0xFF
    spare = bytes(SECTOR_SIZE)
    with args.physical.open("wb") as output:
        offset = 0
        for _cylinder in range(CYLINDERS):
            for _head in range(HEADS):
                size = DATA_SECTORS * SECTOR_SIZE
                output.write(logical[offset : offset + size])
                output.write(spare)
                offset += size


if __name__ == "__main__":
    main()
