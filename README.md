# CPM8000 — a CP/M-8000 development environment

A hosted emulator, build system, and toolchain for CP/M-8000 (Digital
Research's Z8000 CP/M). The Olivetti M20 (Z8001) is the reference board, but
the BIOS and build system are structured so that bringing up other
CP/M-8000 boards is a well-defined task, not a rewrite.

The system is built from the original Zilog/Digital Research CP/M-8000
sources, with the goal of rebuilding the BIOS and utilities from source
using the original DR/Zilog toolchain (C compiler, assembler, linker)
running inside the emulator.

There are three separate BIOSes, all with sources under `src/`:

- **`src/bios/emu/`** — a thin BIOS written for the emulator (bootstrap + trap
  handler); it dispatches BDOS/BIOS calls to host services rather than
  emulating hardware. This is the BIOS the emulator runs.
- **`src/cpm8k/`** — the original Digital Research / Olivetti M20 CP/M-8000
  BIOS (`bios.c` plus `.8kn` assembly), from the distribution disks. This is
  what the from-source build produces.
- **`src/bios/z8001/`** — a BIOS for real Z8001 hardware (the CPM8000 board).
  It is essentially 4sun5bu's [Z8001MB](https://github.com/4sun5bu/Z8001MB)
  BIOS (MIT) — the BIOS dispatch and memory configuration are Z8001MB's —
  with the system origin, I/O port addresses, and serial/IDE config changed
  for the board. It is *not* the Olivetti M20 BIOS; see
  `src/bios/z8001/README.md` and `CHANGES.md`.

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
build/emu/cpm8k -d A=img:distribution/CPM_8000_1.1/REL11A.IMG   # drive A = CP/M disk image
```

All build artifacts go into `build/`.

### Drives

Each drive (`A`..`P`) is mapped independently to either a host directory or a
CP/M-8000 disk image with `-d X=dir:PATH` or `-d X=img:PATH`; the two backends
can be mixed in one session, e.g.:

```
build/emu/cpm8k -d A=img:distribution/CPM_8000_1.1/REL11A.IMG -d C=dir:drives/C
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
    bios/
      z8001/          BIOS for real hardware (adapted from 4sun5bu/Z8001MB)
      emu/            thin BIOS for the emulator (assembly)
  z8000_emu/          Z8001 CPU emulator library
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
4. **Assemble BIOS** — assembles all `.s` files in `src/bios/z8001/` with `z8k-coff-as`
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
(`asz8k.pd`) is the one from the product distribution, which is correct.
An earlier copy carried over from an Alcyon cross-compiler experiment was
corrupt — its FMSKEL2 flag bit was shifted (`100h`→`10h`) on 59 two-word
instructions, so `asz8k` silently emitted `LDM`/`LDIR`/`LDIRB` (and the
block/string ops) as truncated 2-byte instructions instead of 4-byte. That
corrupt predef has been replaced with the correct distribution version.

### Provenance: regenerating `src/cpm8k`

`src/cpm8k/` is not hand-maintained — it is *regenerated* from the pristine
distribution disk images in two auditable steps, so exactly what deviates from
the shipped product is explicit:

```
make regenerate   # step 1: extract the 75 pristine files from the six M20
                  #         images (distribution/CPM_8000_1.1/*.IMG) with
                  #         cpmtools + src/diskdefs_m20.mame
make overlay      # step 2: drop in the one deviation the build needs --
                  #         the from-source linker src/linker/ld8k.z8k
make cpm8k-src    # both steps
```

The **pristine images are the untouched ground truth**; the **overlay is the
sole deviation**. That deviation is only the linker: the shipped `ld8k` works
for `-w` final links but fails `-r` relocatable links in the emulator
("p2 can't open <obj>"), so the overlay supplies the from-source rebuild
(`src/linker/ld8k.z8k`, a build-once stable binary — rebuild it with
`scripts/build-ld8k.sh` when `src/linker/ld8k.c` changes). The assembler predef
`asz8k.pd` is pristine (it matches the image; not a deviation).

Regenerating drops two files that were never on any distribution disk
(`bdos.h`, a stray header used only by the unbuilt `putboot.c`; and
`tmptic.z8k`, a temp build artifact). Building from the regenerated tree
reproduces `bios.rel`, `cpm.sys`, and `cpmldr.sys` **byte-identical**.

### Building the BIOS from source (in the emulator)

`scripts/build-bios.sh` rebuilds the M20 BIOS from these sources using the
original DR/Zilog toolchain *running inside the emulator* (`zcc`, `asz8k` →
`xcon`, `ld8k`, `ar8k`). It stages the sources and toolchain into a fresh
temporary directory, mounts it as drive `C:`, runs `scripts/bios.sub`, and
copies the results out:

```
make emu bios-emu          # build the emulator + its cpm.sys once
scripts/build-bios.sh      # -> build/bios-src/bios.rel, bios.a
```

The result is byte-identical to the distribution's prebuilt `bios.rel`.
`scripts/build-cpmsys.sh` goes one step further and links `bios.rel` with
the CCP+BDOS (`cpmsys.rel`) and `libcpm.a` into `cpm.sys`; its code segment
is byte-identical to the distribution's `cpm.sys`.

## Linker (ld8k) and the symbol-table fix

The linker source lives in `src/linker/` (`ld8k.c` plus its headers) and is
rebuilt from source with `scripts/build-ld8k.sh` (a bootstrap build inside
the emulator). Two points of provenance are worth recording:

- The `ld8k.z8k` shipped in the distribution is **V1.01j**, an *earlier*
  linker than the one that actually built the distribution. It links, but
  produces a divergent `cpm.sys` (~49 KB, different segment layout). The
  linker that reproduces the distribution's code is a later (V1.6-class)
  linker, which is what `src/linker/ld8k.c` builds — it yields a `cpm.sys`
  whose code segment and every global/absolute symbol are byte-identical to
  the distribution.

`ld8k.c` had two bugs in its **symbol-table** (debug) output. Both are in the
final (`-w`) link path only and are gated on `!saverel`, so relocatable
(`-r`) links — and therefore `bios.rel` — stay byte-identical to the
distribution:

1. **Absolute symbols** (segment 255) were relocated through an out-of-bounds
   `segtab[255]` read (`SEGNO` is 128), adding garbage (`0x358`) to every
   absolute symbol value.
2. **Local (debug) symbols** were not relocated like globals across modules,
   leaving them under-relocated in multi-module links.

The correct model was confirmed against Digital Research's own debugger: the
CP/M-68K DDT sources (`disas.h`) define the symbol flags `SYEQ` (equated /
absolute), `SYTX`/`SYDA`/`SYBS` (text/data/bss-relative) and `SYGL` (global).
The relocation basis is **per-segment and orthogonal to the global/local
binding** — a text-relative symbol is relocated by the text base whether it
is global or local, and equated symbols are never relocated. The Z8000 x.out
format is the same design with a different entry layout (a `seg` byte plus a
`type` byte instead of one flag word).

Under that model the fixed linker is correct and the **distribution's own
local symbols are wrong**: e.g. the text-relative local `Fsub10` must carry
its offset in the text segment (`0x0062`, where the byte-identical code
places it), which the fixed linker emits, but the distribution stored
`0x0b9c`. These are debug-only symbols — the relocation table is empty in a
fully-linked `cpm.sys` and the loader never reads the symbol table — so the
difference never affected the running system, only symbolic debugging.

## xoututils

The `src/xoututils/` directory contains C tools for working with the Zilog
x.out object file format used by CP/M-8000:

- **xarch** — extract members from an x.out archive (`.a`)
- **xout2coff** — convert an x.out object file to Z8k-COFF format
- **xoutdump** — dump x.out file headers, segments, relocations, and symbols

These are a C port of the Go tools by 4sun5bu
([xoututils](https://github.com/4sun5bu/xoututils), MIT license).

## BIOS

The `src/bios/z8001/` directory is 4sun5bu's Z8001MB BIOS adapted for the
CPM8000 board — a Z8001MB-based design. The BIOS dispatch and memory
mapping come straight from Z8001MB (`bios.s`, `biosmem.s`, etc. are
unchanged); the changes are the system origin, I/O port addresses, and
serial/IDE configuration. See `src/bios/z8001/CHANGES.md` for the summary and
`src/bios/z8001/z8001mb-to-m20.patch` for the full diff. This is **not** the
original Olivetti M20 BIOS — those sources are in `src/cpm8k/`.

## Acknowledgments

- **4sun5bu** — original [Z8001MB](https://github.com/4sun5bu/Z8001MB) BIOS
  and [xoututils](https://github.com/4sun5bu/xoututils) (MIT license)
- **Digital Research** — CP/M-8000 1.1, licensed by Lineo, Inc.
  (see [The Unofficial CP/M Web Site](http://www.cpm.z80.de/))

## License

BSD 2-Clause — see [LICENSE](LICENSE).

CP/M-8000 system files (`cpmsys.rel`, `libcpm.a`) are licensed by Lineo, Inc.
