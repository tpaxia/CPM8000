# CPM8000 — a CP/M-8000 development environment

A hosted emulator, build system, and toolchain for CP/M-8000 (Digital
Research's Z8000 CP/M). The Olivetti M20 (Z8001) is the reference board, but
the BIOS and build system are structured so that bringing up other
CP/M-8000 boards is a well-defined task, not a rewrite.

The system is built from the original Zilog/Digital Research CP/M-8000
sources, with the goal of rebuilding the BIOS and utilities from source
using the original DR/Zilog toolchain (C compiler, assembler, linker)
running inside the emulator.

There are two kinds of BIOS.

**Emulator infrastructure** (not a sysgen target):

- **`src/cpm8kemu/bios/`** — a thin BIOS written for the emulator (bootstrap +
  trap handler); it dispatches BDOS/BIOS calls to host services rather than
  emulating hardware. Built by `make bios-emu`; this is the BIOS the emulator
  runs.

**Pluggable BIOSes** under `src/bios/` — each is a package with a `Makefile`
that `sysgen` builds into a bootable system (see [System generation](#system-generation-sysgen)):

- **`src/bios/m20/`** — the M20 reference BIOS, built with the in-emulator
  DR/Zilog toolchain (`zcc`, `asz8k`, `ld8k`). An *overlay* package: the BIOS
  sources come from the stock M20 tree (`src/cpm8k/` — `bios.c` plus `.8kn`
  assembly, from the distribution disks); the package overlays only the files
  it changes (nothing, for the stock reference).

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
      bios/           thin BIOS for the emulator (assembly) -- infrastructure
    xoututils/        C tools to convert Zilog x.out format to Z8k-COFF
    linker/           ld8k linker source (+ committed overlay binary)
    asm8k/            asz8k assembler source (from-source build)
    bios/             pluggable BIOSes (each a Makefile package)
      m20/            M20 reference BIOS (overlay on src/cpm8k)
  z8000_emu/          Z8001 CPU emulator library
  build/
    system/<name>/    generated systems (cpm.sys [, cpmldr.sys]) from sysgen
    ...               other build output (created by make)
  Makefile            top-level build pipeline
  LICENSE             BSD 2-Clause
```

## Build pipeline

The top-level `make` builds the emulator, in these steps:

1. **Build tools** — compiles `xarch` and `xout2coff` in `src/xoututils/`
2. **Convert library** — extracts the members from the Zilog x.out `libcpm.a`,
   converts each to Z8k-COFF, and packs them into a new `build/lib/libcpm.a`
   with `z8k-coff-ar`
3. **Convert object** — converts the CCP+BDOS object `cpmsys.o` from x.out to COFF
4. **Assemble the emulator BIOS** — assembles the thin BIOS in
   `src/cpm8kemu/bios/` and links `build/bios-emu/cpm.sys`
5. **Build the emulator** — compiles the host program in `src/cpm8kemu/`

Bootable CP/M-8000 systems are then generated with `make system` (see below).

### Make targets

| Target | Description |
|--------|-------------|
| `make` | Build the emulator (tools + lib + BIOS + host) |
| `make tools` | Build xoututils only |
| `make lib` | Convert library and object only |
| `make bios-emu` | Assemble thin BIOS for emulator |
| `make emu` | Build the hosted emulator binary |
| `make system NAME=<n>` | Generate a bootable system for a BIOS (see below) |
| `make regenerate` / `overlay` / `cpm8k-src` | Regenerate `src/cpm8k` from the images (+ overlay) |
| `make clean` | Remove `build/` |

## System generation (sysgen)

`sysgen` turns a **BIOS** into a bootable CP/M-8000 system. A BIOS is a
directory under `src/bios/` containing a `Makefile` (a "BIOS package"); that's
the whole recognition contract. `sysgen` runs the package's Makefile to build
the BIOS object, then does the final system link into `build/system/<name>/`.

```
make emu bios-emu                 # one-time: build the emulator + its cpm.sys
make system NAME=m20              # -> build/system/m20/cpm.sys
make system NAME=m20 LOADER=1     # -> also build/system/m20/cpmldr.sys
make system NAME=foo BIOS=src/bios/foo   # explicit BIOS dir
```

The BIOS is built with the in-emulator DR/Zilog toolchain (`zcc`, `asz8k`,
`ld8k`), producing x.out objects.

**The BIOS-package contract** (see `src/bios/m20/Makefile`):

- `make -C src/bios/<name> bios.rel BUILDDIR=<dir>` builds the BIOS object.
- A package is an **overlay**: BIOS sources come from the stock M20 tree
  (`src/cpm8k`), and the package overlays only the `.c`/`.8kn` files it changes
  — the same overlay idea as regenerating `src/cpm8k`. The stock M20 reference
  has an empty overlay.

**To bring up a custom board:** copy `src/bios/m20/` to `src/bios/<board>/`,
drop in your changed `.c`/`.8kn` (and edit `biosasm.8kn`'s `.input` list if you
add files), then `make system NAME=<board>`.

The `cpm.sys` final link (`bios.rel` + `cpmsys.rel` + `libcpm.a` via `ld8k`)
lives in `scripts/link-cpmsys.sh`; the optional loader is built by
`scripts/build-cpmldr.sh`.

## CP/M-8000 sources

The `src/cpm8k/` directory contains the CP/M-8000 system files from the
Zilog CP/M-8000 1.1 product distribution disk. 

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

## Acknowledgments

- **4sun5bu** — [xoututils](https://github.com/4sun5bu/xoututils) (MIT license)
- **Digital Research** — CP/M-8000 1.1, licensed by Lineo, Inc.
  (see [The Unofficial CP/M Web Site](http://www.cpm.z80.de/))

## License

BSD 2-Clause — see [LICENSE](LICENSE).

CP/M-8000 system files (`cpmsys.rel`, `libcpm.a`) are licensed by Lineo, Inc.
