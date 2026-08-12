# CPM8000 — a CP/M-8000 development environment

CPM8000 combines the original Digital Research/Zilog CP/M-8000 software with a
hosted Z8000 emulator, reproducible source reconstruction, the original guest
toolchain, target-oriented system generation, and logical CP/M media creation.
The Olivetti M20 is the reference Z8001 target. The hosted environment can run
CP/M in either Z8001 or Z8002 mode.

## Quick start

After installing the [build prerequisites](#prerequisites), clone the repository
with its Z8000 CPU-emulator submodule, build the default hosted Z8001 system,
and run it with the complete CP/M-8000 development tree mounted as drive C:

```sh
git clone --recurse-submodules https://github.com/tpaxia/CPM8000.git
cd CPM8000
make
build/emu/cpm8k-z8001 \
  -d C=dir:src/cpm8k
```

The emulator starts at the CP/M `C>` prompt with the original tools, sources,
headers, libraries, examples, and `.sub` build recipes available. Type `exit`
to leave it. Run the Z8002 hosted system with:

```sh
make bios-emu-z8002 emu
build/emu/cpm8k-z8002 \
  -d C=dir:src/cpm8k
```

Both modes can mount the same directory-backed development tree or the same
CP/M filesystem image, but they do not run the same system binary. Before
starting the emulated CPU, the host loads the matching CPU-specific system:
`build/bios-emu-z8001/cpm.sys` for `cpm8k-z8001` or
`build/bios-emu-z8002/cpm.sys` for `cpm8k-z8002`. These files are not loaded from
the mounted CP/M drive, so a `CPM.SYS` stored there does not select the running
system.

The separate [Z8002-demo BIOS documentation](src/bios/z8002-demo/README.md)
describes its MAME machine, monitor, native system, and disk image.

## Overview

The project keeps four jobs separate:

1. **Source reconstruction** recreates the distribution tree and applies a
   small, explicit maintained-source overlay.
2. **The hosted emulator** runs CP/M-8000 and its original compiler, assembler,
   linker, and utilities without emulating a complete historical computer.
3. **System generation (`sysgen`)** compiles a target BIOS and links guest
   `cpm.sys` and, optionally, `cpmldr.sys` binaries.
4. **Development-media generation** copies the common source/tool environment
   plus a target's BIOS source overlay into declared logical CP/M filesystem
   formats.

This separation provides several practical advantages:

- Z8001 and Z8002 execution can be compared against identical files and build
  commands.
- Original guest tools produce the final Zilog x.out binaries; host conversion
  is limited to the thin systems required to run the hosted emulator.
- Distribution files, reconstructed sources, and local fixes have auditable
  provenance.
- Compilers, assemblers, linkers, utilities, and examples are common content,
  rather than being duplicated in every hardware package.
- A hardware package declares compatible filesystem formats without mixing in
  boot installation or emulator-container conversion.
- Directory-backed drives provide a fast edit/build loop, while image-backed
  drives exercise the native CP/M BDOS and BIOS sector path.

The principal sections are independently usable:

- [Sources and format conversions](#sources-and-format-conversions)
- [Hosted emulator: build and execution](#hosted-emulator-build-and-execution)
- [System generation and development media](#system-generation-and-development-media)
- [Component notes](#component-notes)

## Sources and format conversions

This section explains where the source tree comes from and which files are
converted for host execution.  It does not require familiarity with sysgen or
media generation.

### Distribution and maintained overlay

The six images under `distribution/CPM_8000_1.1/` are the ground truth for
`src/cpm8k`.  Regeneration has two explicit stages:

```sh
make regenerate  # extract the 76 pristine files from the six M20 images
make overlay     # apply maintained linker and reconstructed FPE sources
make cpm8k-src   # perform both stages
```

`make regenerate` produces only distribution files.  `make overlay` adds or
replaces:

| File | Reason |
|------|--------|
| `ld8k.z8k` | From-source linker that supports the required `-r` path |
| `fpe.8kn` | Maintained software floating-point emulator source |
| `fpedep.8kn` | Reconstructed Z8001/M20-dependent FPE support |
| `biosdefs.z8k` | Segmented trap-frame definitions used by FPE |
| `fpe.sub` | In-guest FPE rebuild recipe |

The overlay is reproducible from `src/linker`, `src/fpe`, and `scripts/fpe.sub`.
The FPE core will eventually be separated from its Z8001/M20-dependent half so
that a Z8002 adaptation can be built cleanly.

### Original guest formats and host conversion

CP/M-8000 uses Zilog x.out objects, archives, and executables.  The original
DR/Zilog tools continue to run inside CP/M and produce that format.  Host tools
under `src/xoututils` are used only where the hosted emulator needs COFF:

- `xarch` extracts members from an x.out archive.
- `xout2coff` converts x.out objects to Z8k-COFF.
- `xout2flat` converts a linked native system to the flat monitor payload.
- `xoutdump` reports x.out headers, segments, relocations, and symbols.

The hosted systems are assembled and linked on the host as follows:

| Hosted CPU | CCP/BDOS input | Thin hosted BIOS | Host-loaded system |
|------------|----------------|------------------|--------------------|
| Z8001 | `src/cpm8k/cpmsys.rel` | `src/cpm8kemu/bios-z8001/` | `build/bios-emu-z8001/cpm.sys` |
| Z8002 | `src/cpm8k/cpmsys2.rel` | `src/cpm8kemu/bios-z8002/` | `build/bios-emu-z8002/cpm.sys` |

These two `cpm.sys` files are Z8k-COFF executables loaded by the host before
the CPU starts.  They are not read from a CP/M drive.

### Source layout

```text
src/cpm8k/             regenerated distribution tree plus maintained overlay
src/cpm8kemu/          hosted emulator and its two thin BIOSes
src/asm8k/             assembler source
src/linker/            linker source and maintained guest binary
src/fpe/               maintained FPE sources and provenance
src/xoututils/         host-side x.out inspection/conversion tools
src/bios/<target>/     target BIOS source overlays and package Makefiles
src/media/<format>/    logical CP/M filesystem format descriptors
```

## Hosted emulator: build and execution

The hosted emulator is a CP/M build and validation environment.  It emulates
the Z8000 CPU and CP/M trap ABI, while host services provide console, files,
and disk-image sector I/O.  It is distinct from a complete M20 hardware
emulator.  The [hosted-emulator architecture](src/cpm8kemu/ARCHITECTURE.md)
describes how the shared implementation handles Z8001 segmented and Z8002
non-segmented execution, SC frames, address translation, and TPA allocation.

### Prerequisites

- Z8k COFF binutils (`z8k-coff-as`, `z8k-coff-ld`, `z8k-coff-ar` and related
  tools).  The tested fork is
  [tpaxia/binutils-2.46.0](https://github.com/tpaxia/binutils-2.46.0).
- A C++17 compiler.
- CMake 3.16 or newer.
- `cpmtools` for logical disk-image generation and inspection.

Fetch the CPU-emulator submodule after a new checkout:

```sh
git submodule update --init --recursive
```

### Building

```sh
make                       # both hosted CPUs and their thin BIOSes
make bios-emu-z8001 emu    # explicit Z8001 build
make bios-emu-z8002 emu    # also build the Z8002 hosted system
```

`make bios-emu` remains a compatibility alias for `bios-emu-z8001`.

The build first creates `xarch` and `xout2coff`, converts `libcpm.a` and the
selected CCP/BDOS object, links the matching thin hosted BIOS, and builds
the `build/emu/cpm8k-z8001` and `build/emu/cpm8k-z8002` executables with CMake.
There is no mode-neutral executable: the name always identifies the CPU model.

### Running Z8001 and Z8002

At least one drive must be configured:

```sh
# Hosted Z8001 (the default)
build/emu/cpm8k-z8001 -d C=dir:src/cpm8k

# Hosted Z8002 using the same files
build/emu/cpm8k-z8002 -d C=dir:src/cpm8k

# Native CP/M filesystem image
build/emu/cpm8k-z8002 \
  -d A=img:distribution/CPM_8000_1.1/REL11A.IMG
```

Each drive `A` through `P` can independently use `dir:PATH` or `img:PATH`.
The smallest configured letter becomes the initial default drive.  A
directory-backed drive routes file operations to the host; an image-backed
drive runs the native BDOS and BIOS sector path.

The executable selects its matching host-loaded system at build time. A guest
`CPM.SYS` created by a submit file or visible on a mapped drive is only a guest
file and does not replace the running hosted system. Before the CP/M sign-on,
the hosted emulator also prints the active CPU mode.

The Z8001 and Z8002 implementations have been validated by running the same
ten submit pipelines (`ASZ8K`, `LD8K`, `FPE`, `BIOS`, `CPMSYS`, `LINKSYS`,
`MAKELDR`, `MKPUTBT`, `WUMP`, and `TICTAC`) and comparing every resulting file
byte-for-byte. `make submit-regression` repeats this validation for both CPUs
and checks the resulting binaries against `tests/submit-regression.sha256`.
That manifest was recorded with the pre-refactor Z8001 emulator at commit
`075c8e9`. Recording a replacement requires both `--record` and an explicit
known-good executable in `CPM8K_EMU`; a normal test never accepts new hashes.

## System generation and development media

This section describes two related but separate operations.  Sysgen produces
target-specific guest binaries; media generation packages a development tree
in a target-declared logical filesystem format.  Neither operation installs a
boot sector.

### Sysgen

Sysgen generates both segmented Z8001 and non-segmented Z8002 systems. Each
BIOS package selects the hosted build CPU and system substrate (`cpmsys.rel`
or `cpmsys2.rel`); the original compiler, assembler, and linker run inside
that hosted CP/M environment:

```text
stock BIOS sources from src/cpm8k + src/bios/<name>/ source overlay
                               |
                    in-guest SUBMIT BIOS
                               v
                  build/bios/<name>/bios.rel
                               +
             selected CP/M system object and libcpm.a
                               |
                  in-guest SUBMIT LINKSYS
                               v
                 build/system/<name>/cpm.sys
```

Commands and outputs:

```sh
make system NAME=m20
# build/bios/m20/bios.rel
# build/system/m20/cpm.sys

make system NAME=m20 LOADER=1
# also build/system/m20/cpmldr.sys

make system NAME=foo BIOS=src/bios/foo
```

`NAME` chooses `src/bios/<name>` by default and names both output directories.
`BIOS=<dir>` selects another package.  `LOADER=1` builds the cold-boot loader,
but does not run `putboot` or create bootable media.

The BIOS-package build contract is:

```sh
make -C src/bios/<name> bios.rel BUILDDIR=<directory>
```

The package is a source overlay: distribution BIOS sources are staged from
`src/cpm8k`, then any `.c` or `.8kn` files in the package replace or extend
them. Development media also apply package `.sub` overrides for target-specific
links. The package's `CPMSYS` and `EMU_MODEL` metadata select the system object
and hosted CPU. The [BIOS package guide](src/bios/README.md) describes the
stock `m20` package and the `m20-serial` variant; the
[`z8002-demo` package](src/bios/z8002-demo/README.md) documents its complete
native-system and MAME workflow. A target can also provide a
`system-artifacts` rule for additional target-specific outputs.

### Logical development media

Development media contain the common CP/M-8000 tools, sources, headers,
libraries, examples, and self-contained submit files. A target package adds
its BIOS source and submit overrides and declares compatible filesystem formats
through its `media-formats` Make target.

List formats for the M20 package:

```sh
make media-formats NAME=m20
# m20-floppy-set m20-hd
```

Build either logical format:

```sh
make media NAME=m20 FORMAT=m20-floppy-set
make media NAME=m20 FORMAT=m20-hd
```

| Format | Output | Description |
|--------|--------|-------------|
| `m20-floppy-set` | `build/media/m20/m20-floppy-set/development-01.img` … `development-06.img` | Common tree split deterministically across six 280 KiB M20 CP/M filesystems |
| `m20-hd` | `build/media/m20/m20-hd/development.img` | One 8,839,168-byte logical M20 CP/M hard-disk filesystem |

Every image is created at its full declared logical geometry and checked with
`fsck.cpm`.  These are CP/M filesystem images only:

- no generated `cpm.sys` or `cpmldr.sys` is installed;
- no boot sector is written because that requires the target's `putboot`
  procedure;
- no CHD or other emulator-specific container is generated.

The reusable format contract is documented in
[`src/media/README.md`](src/media/README.md).  A target advertises compatible
formats in its package Makefile; a format descriptor supplies the cpmtools
diskdef, single-versus-split layout, output stem, and exact image size.

## Component notes

This section records details useful when changing individual components.  The
earlier sections are sufficient for ordinary source, emulator, sysgen, and
media use.

### Linker

The distribution `ld8k.z8k` is V1.01j and does not reproduce the linker used
to build the shipped system.  `src/linker/ld8k.c` builds a later V1.6-class
linker and fixes two symbol-table-only problems: absolute symbols were
relocated through an invalid segment-table entry, and local debug symbols were
not relocated by their segment bases.  These corrections do not alter fully
linked runtime code.

Rebuild the maintained guest linker with:

```sh
scripts/build-ld8k.sh
```

### Floating-point emulator

`src/fpe/fpe.z8k` implements the EPA extended-instruction trap emulator.
`fpedep.z8k` supplies system-dependent memory and trap-frame operations.  The
current dependent half reproduces the segmented Z8001/M20 object; it is not yet
the Z8002 adaptation.

```sh
scripts/build-fpe.sh
```

The reconstructed `fpedep.o` content matches the distribution object.  The
rebuilt `fpe.o` differs only in an uninitialized `.block` work area that the
emulator initializes at runtime.  See [`src/fpe/README.md`](src/fpe/README.md)
for provenance and variant analysis.

### Further implementation notes

[`PROGRESS.md`](PROGRESS.md) contains lower-level emulator, trap, loader, and
filesystem implementation notes.

## Acknowledgments

- 4sun5bu — [xoututils](https://github.com/4sun5bu/xoututils) (MIT license)
- Digital Research — CP/M-8000 1.1, licensed by Lineo, Inc. (see
  [The Unofficial CP/M Web Site](http://www.cpm.z80.de/))

## License

BSD 2-Clause — see [LICENSE](LICENSE).

CP/M-8000 system files (`cpmsys.rel`, `cpmsys2.rel`, and `libcpm.a`) are
licensed by Lineo, Inc.
