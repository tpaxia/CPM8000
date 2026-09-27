# Plasmo BIOS

This package builds CP/M-8000 for **Plasmo**, a real homebrew Z8002
single-board computer documented by its designer on the
[VCFED "A homebrew Z8002" forum thread](https://forum.vcfed.org/index.php?threads/a-homebrew-z8002.1253969/)
(read through page 6, the latest revision as of 2025-10-05). It is physical
hardware; a MAME driver modeled from its schematics exists for testing
(see "Status and open items").

It uses a CPLD-synthesized UART (no discrete UART chip) for the console, a
CompactFlash ATA task-file interface for disk I/O, and a single 2-bit
pseudo-segment banking register (port `$85`) for CP/M's SC #1 memory
services. It selects the same original non-segmented `cpmsys2.rel` that
Z8002-demo uses, and targets the Z8002 (`TARGET_CPU`); its guest build runs on
the hosted Z8002 by default, or on the hosted Z8001 with `HOST_CPU=z8001`.

See [TPA.md](TPA.md) for the complete memory model, why this hardware can
only run merged (not split-I/D) transient programs, and the hard
instruction-fetch-safety constraint the aperture code has to satisfy.

The Zilog C compiler, assembler and linker need separate 64 KiB spaces for
code and data, but Plasmo gives a program only one; the board has enough RAM
for this (256 KiB fitted, only 128 KiB used today), so an updated CPLD that
addresses all of it and sends a program's instruction fetches and data
accesses to different banks would let the compiler run natively.

## Hardware summary

- **CPU**: Z8002 (non-segmented), all I/O via register-indirect `IN`/`OUT` —
  some early-date (1979) Z8002 samples hang on direct-addressed I/O, so this
  BIOS never uses it, matching the board's own monitor.
- **Memory**: 256 KiB RAM fitted, of which the CPLD addresses 128 KiB as four
  fixed 32 KiB quarters (System-Lo, System-Hi, Normal-Lo, Normal-Hi). System-Hi is switchable via port `$85` to peek into
  either TPA half; Normal mode (the TPA) is always a fixed, unbanked 64 KiB.
- **Console**: CPLD UART at `$81` (data) / `$83` (status: bit0=RxRdy,
  bit1=TxEmpty), fixed 115200 N81, no interrupts.
- **Disk**: CompactFlash at `$10-$1E`, standard ATA task-file register
  layout, LBA addressing mode.
- **Boot ROM**: `$0-$FF`, present at reset; writing `$87` disables it
  permanently and `$0000-$FFFF` becomes RAM.

All of the above is sourced from the board's own monitor source
(`Z8002mon.txt`) and CPLD/memory-map schematics, not forum prose — see
TPA.md's "Hardware source" section.

Build the system with:

```sh
make system NAME=plasmo
```

The outputs are:

- `build/system/plasmo/cpm.sys`: native non-segmented CP/M system;
- `build/system/plasmo/plasmo.boot`: system padded to a 128-sector
  (64 KiB) boot payload, in the same layout convention as Z8002-demo.

## Status and open items

Tested under a MAME driver written from the schematics
(`plasmo` in the `z8002-plasmo` branch of `tpaxia/mame`, see
`PLASMO_TESTING.md`) with the CF image from
`make plasmo-image`: boot to `A>`, `dir`, `type`, `stat` (a transient),
and `pip` copy with read-back all work. Not yet run on the physical board.

Remaining, separate from this BIOS:

1. **Getting the system image onto the board.** The board's current monitor
   (`Z8002mon.txt`, 2025-09-13) predates this BIOS's memory scheme and has
   no command that loads a full system image and jumps to it (see TPA.md).
   The MAME driver sidesteps this with `-quik`; it does not model the real
   (undumped) CPLD bootstrap.
2. **CF layout on real hardware.** `scripts/build-plasmo-hd.sh` uses the
   Z8002-demo layout (system in LBA 0-127, filesystem from LBA 128); it
   hasn't been checked against what a real Plasmo boot path will expect.
