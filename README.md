# CPM8000 — a CP/M-8000 development environment

A hosted emulator, build system, and toolchain for CP/M-8000 (Digital
Research's Z8000 CP/M). The Olivetti M20 (Z8001) is the reference board, but
the BIOS and build system are structured so that bringing up other
CP/M-8000 boards is a well-defined task, not a rewrite.

The system is built from the original Zilog/Digital Research CP/M-8000
sources, with the goal of rebuilding the BIOS and utilities from source
using the original DR/Zilog toolchain (C compiler, assembler, linker)
running inside the emulator.

The BIOS is derived from [4sun5bu/Z8001MB](https://github.com/4sun5bu/Z8001MB),
adapted for the M20 hardware (different I/O addresses, serial configuration,
IDE interface, memory map).

## Emulator

The project includes a hosted emulator that runs CP/M-8000 natively on
an emulated Z8001 CPU, with BDOS and BIOS services provided by the host.
Drives are mapped to host directories under `drives/`.

```
make emu
build/emu/cpm8k
```

The emulator uses the Z8001's native trap mechanism for all system calls.
SC instructions trigger hardware traps that dispatch to BIOS assembly
handlers, which bridge to C++ via I/O port OUT instructions. This allows
xfer (context transfer) to switch segments correctly via IRET.

See [PROGRESS.md](PROGRESS.md) for detailed architecture documentation.

## Prerequisites

- **z8k-coff binutils** — GNU Binutils with `--target=z8k-coff`;
  fork at [tpaxia/binutils-2.46.0](https://github.com/tpaxia/binutils-2.46.0)
- **C++17 compiler** — for the Z8001 emulator and host program

## Quick start

```
make            # builds tools, converts library, assembles BIOS
make bios-emu   # build thin BIOS + CCP for emulator
make emu         # build the hosted emulator binary

# run the emulator (at least one drive must be mapped)
build/emu/cpm8k -d A=dir:drives/A                  # drive A = host directory
build/emu/cpm8k -d A=img:distribution/REL11A.IMG   # drive A = CP/M disk image
```

All build artifacts go into `build/`.

### Drives

Each drive (`A`..`P`) is mapped independently to either a host directory or a
CP/M-8000 disk image with `-d X=dir:PATH` or `-d X=img:PATH`; the two backends
can be mixed in one session, e.g.:

```
build/emu/cpm8k -d A=img:distribution/REL11A.IMG -d C=dir:drives/C
```

Drives need not start at `A`. The system boots with the **smallest configured
drive letter** as the default drive, so `-d B=... -d C=...` comes up at the
`B>` prompt (not `A>`, which would fail since `A` isn't mapped). Command-line
order doesn't matter — the lowest letter wins.

Host-directory drives are serviced at the BDOS file level against the host
filesystem; image drives run the real CP/M-8000 BDOS doing sector I/O against
the image file. See [PROGRESS.md](PROGRESS.md) for the current status of each
backend and the debug/trace flags (`-b`, `-t`, `-r`, `-m`, `-v`).

## Project structure

```
CPM8000/
  src/
    cpm8k/            CP/M-8000 sources from Zilog product distribution
    cpm8kemu/         hosted emulator (C++17, runs on macOS/Linux)
    xoututils/        C tools to convert Zilog x.out format to Z8k-COFF
  z8000_emu/          Z8001 CPU emulator library
  bios/
    z8001/            BIOS for real hardware (adapted from 4sun5bu/Z8001MB)
    emu/              thin BIOS for the emulator (assembly)
  build/              all build output (created by make)
  Makefile            top-level build pipeline
  LICENSE             BSD 2-Clause
```

## Build pipeline

The top-level `make` runs these steps in order:

1. **Build tools** — compiles `xarch` and `xout2coff` in `src/xoututils/`
2. **Convert library** — extracts 129 members from the Zilog x.out `libcpm.a`,
   converts each to Z8k-COFF, and packs them into a new `build/lib/libcpm.a`
   with `z8k-coff-ar`
3. **Convert objects** — converts `fpe.o`, `fpedep.o`, and `cpmsys.o` from
   x.out to COFF
4. **Assemble BIOS** — assembles all `.s` files in `bios/z8001/` with `z8k-coff-as`
5. **Link** — links BIOS objects, `cpmsys.o`, and `-lcpm` into `cpm8k` COFF,
   then strips to raw binary `cpm8k.bin`

### Make targets

| Target | Description |
|--------|-------------|
| `make` | Full build (convert + assemble + link) |
| `make tools` | Build xoututils only |
| `make lib` | Convert library and objects only |
| `make bios` | Assemble and link BIOS (real hardware) |
| `make bios-emu` | Assemble thin BIOS for emulator |
| `make emu` | Build the hosted emulator binary |
| `make clean` | Remove `build/` |

## CP/M-8000 sources

The `src/cpm8k/` directory contains the CP/M-8000 system files from the
Zilog CP/M-8000 1.1 product distribution disk. The assembler predef
(`asz8k.pd`) has been corrected to fix the FMSKEL2 flag values (the
product disk had shifted flag bits that caused LDM/LDIR/LDIRB instructions
to be assembled incorrectly).

## xoututils

The `src/xoututils/` directory contains C tools for working with the Zilog
x.out object file format used by CP/M-8000:

- **xarch** — extract members from an x.out archive (`.a`)
- **xout2coff** — convert an x.out object file to Z8k-COFF format
- **xoutdump** — dump x.out file headers, segments, relocations, and symbols

These are a C port of the Go tools by 4sun5bu
([xoututils](https://github.com/4sun5bu/xoututils), MIT license).

## BIOS

The `bios/z8001/` directory contains the CP/M-8000 BIOS adapted from
4sun5bu's Z8001MB project for a machine with M20-like hardware. This is
**not** the original Olivetti M20 BIOS. See `bios/z8001/CHANGES.md` for
details and `bios/z8001/z8001mb-to-m20.patch` for the full diff.

## Acknowledgments

- **4sun5bu** — original [Z8001MB](https://github.com/4sun5bu/Z8001MB) BIOS
  and [xoututils](https://github.com/4sun5bu/xoututils) (MIT license)
- **Digital Research** — CP/M-8000 1.1, licensed by Lineo, Inc.
  (see [The Unofficial CP/M Web Site](http://www.cpm.z80.de/))

## License

BSD 2-Clause — see [LICENSE](LICENSE).

CP/M-8000 system files (`cpmsys.rel`, `libcpm.a`) are licensed by Lineo, Inc.
