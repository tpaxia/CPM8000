# Z8002-demo BIOS

This package builds CP/M-8000 for a non-segmented Z8002-demo machine emulated
by MAME. It uses Z80-SIO channel B for the console, a generic ATA task-file
interface for disk I/O, and a system/normal banking MMU for CP/M's SC #1 memory
services. A compatible physical implementation exists in
`Z8000_FPGA/z8000_examples/cpm8000_z8002`; it realizes the ATA interface with
KFMMC but does not change the BIOS-visible machine contract.

The package selects the original non-segmented `cpmsys2.rel` and runs its guest
build under the hosted Z8002 emulator.  Its Memory Region Table uses the
representation expected by that binary: bank 1 is `0x01000000`, bank 2 is
`0x02000000`, and bank 3 is `0x03000000`.

Its `CPMSYS.SUB` and `LINKSYS.SUB` overrides also select `cpmsys2.rel`, so the
same submit commands can be validated on hosted Z8002 CP/M and on the MAME
machine without accidentally rebuilding a segmented Z8001 system.

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

The outputs are:

- `build/system/z8002-demo/cpm.sys`: native non-segmented CP/M system;
- `build/system/z8002-demo/z8002-demo.boot`: system padded to the 128-sector
  monitor load area;
- `build/media/z8002-demo/z8002-demo-hd/z8002-demo.raw`: exact 8 MiB ATA disk
  for compatible physical implementations;
- `build/media/z8002-demo/z8002-demo-hd/z8002-demo.chd`: uncompressed MAME
  wrapper around that disk.

The disk begins with the native Z8002 `cpm.sys` payload rather than relying on
the hosted emulator's system loader. Its CP/M filesystem contains the common
development files and target-specific BIOS and submit recipes.

The validated monitor is included as `z8kmon.bin`, making the BIOS package
self-contained. System generation copies it to
`build/roms/z8002demo/z8kmon.bin`, the ROM-set layout expected by MAME. The
monitor source remains with the compatible physical implementation under
`Z8000_FPGA/z8000_examples/cpm8000_z8002/z8002-bios`. MAME consumes the CHD;
the physical implementation can consume the corresponding raw image.

## Running in MAME

Build the [`z8002-demo` branch of the tpaxia MAME fork](https://github.com/tpaxia/mame/tree/z8002-demo),
run `make z8002-demo-image` in CPM8000, and then
run the resulting MAME executable from the CPM8000 repository root:

```sh
/path/to/mame z8002demo \
  -rompath build/roms \
  -hard build/media/z8002-demo/z8002-demo-hd/z8002-demo.chd
```

Enter `z` at the monitor prompt to boot CP/M from the ATA disk.
