# Z8002-demo BIOS

This package builds CP/M-8000 for the non-segmented Z8002 FPGA machine in
`Z8000_FPGA/z8000_examples/cpm8000_z8002`.  It uses Z80-SIO channel B for the
console, the KFMMC ATA task-file interface for disk I/O, and the board's
system/normal banking MMU for CP/M's SC #1 memory services.

The package selects the original non-segmented `cpmsys2.rel` and runs its guest
build under the hosted Z8002 emulator.  Its Memory Region Table uses the
representation expected by that binary: bank 1 is `0x01000000`, bank 2 is
`0x02000000`, and bank 3 is `0x03000000`.

Its `CPMSYS.SUB` and `LINKSYS.SUB` overrides also select `cpmsys2.rel`, so the
same submit commands can be validated on hosted Z8002 CP/M and on the native
FPGA/MAME disk without accidentally rebuilding a segmented Z8001 system.

The complete development submit suite has been run from a clean disk under
MAME: `ASZ8K`, `LD8K`, `FPE`, `BIOS`, `CPMSYS`, `LINKSYS`, `WUMP`, and
`TICTAC` all complete successfully. The resulting filesystem passes
`fsck.cpm`; generated tools, objects, applications, `bios.rel`, and `cpm.sys`
match the corresponding canonical builds. `FPE.O` is the original logical
object with CP/M record padding in its final byte.

Build the system and the complete 8 MiB development disk with:

```sh
make system NAME=z8002-demo
make z8002-demo-image
```

The first 128 sectors of the disk hold a padded 64 KiB flat system image.  The
CP/M filesystem begins at LBA 128 and occupies the rest of the disk.

The boot monitor is maintained only in
`Z8000_FPGA/z8000_examples/cpm8000_z8002/z8002-bios`; it is not copied into
this repository. The FPGA consumes `z8002-demo.raw` directly, while MAME uses
the CHD wrapper around the same image and identifies the external monitor ROM
by its checksum.
