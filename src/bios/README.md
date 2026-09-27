# BIOS packages

The directories below this one are source-overlay packages used by `sysgen`.
The build first stages the common M20 CP/M-8000 BIOS sources from `src/cpm8k`,
then applies the selected package's `.c` and `.8kn` files. Development-media
staging also applies package `.sub` overrides. A package therefore contains
only the files and target-specific recipes that differ from the common tree,
plus two one-line metadata files:

- `TARGET_CPU` -- the CPU the system runs on (`z8001` or `z8002`).  It selects
  the target FPE objects and definitions and the development submit recipes.
- `CPMSYS` -- the CCP+BDOS object to link (default `cpmsys.rel`; the Z8002
  packages use `cpmsys2.rel`).

The current packages are:

| Package | Console | Purpose |
|---------|---------|---------|
| `m20` | M20 keyboard and video display | Builds the stock M20 BIOS without source overrides. |
| `m20-serial` | M20 RS-232 terminal port at 9600 baud | Provides a serial console suitable for a host PTY, scripted sessions, and headless testing. |
| `z8002-demo` | Z80-SIO channel B at 9600 baud | Native non-segmented Z8002 BIOS with banking-MMU and ATA support for the FPGA/MAME demo machine. |
| `plasmo` | CPLD UART at 115200 baud | Native non-segmented Z8002 BIOS for the Plasmo homebrew board: CompactFlash on the ATA task file and a single-window bank register. See `plasmo/README.md`. |

## Why `m20-serial` exists

The original M20 BIOS uses the machine's keyboard and video display for the
CP/M console.  That is appropriate for an interactive M20, but it makes build
automation depend on MAME's emulated keyboard and display.  The `m20-serial`
package replaces only `bios.c`: it initializes the M20 terminal RS-232 port at
9600 baud and routes the CP/M `CONST`, `CONIN`, and `CONOUT` BIOS calls through
that port.  MAME can connect the port to a host PTY, allowing commands and
output to be handled reliably by terminal programs and scripts.

This is still an M20 BIOS.  It does not select a different processor, disk
controller, memory layout, or filesystem format.  Its only intentional
platform difference is the console path and the serial-port initialization
needed for that path.

## Building a system

Select a package with `NAME`:

```sh
make system NAME=m20
make system NAME=m20-serial
make system NAME=z8002-demo
make system NAME=plasmo
```

The resulting files are written to:

```text
build/bios/<name>/bios.rel
build/system/<name>/cpm.sys
```

Each package implements this build contract:

```sh
make -C src/bios/<name> bios.rel BUILDDIR=<directory> [HOST_CPU=z8001|z8002]
```

`HOST_CPU` chooses the hosted emulator that runs the guest toolchain; it
defaults to the package's `TARGET_CPU`, and either host produces identical
output.  With `make system`, pass it the same way:

```sh
make system NAME=plasmo HOST_CPU=z8001
make system NAME=m20 HOST_CPU=z8002
```

## Development media

Create a persistent host-backed development drive with conventional CP/M
filenames and submit commands for one package:

```sh
make dev NAME=m20          # drives/dev-m20
make dev NAME=z8002-demo   # drives/dev-z8002-demo
make dev NAME=plasmo       # drives/dev-plasmo
```

`make dev-z8001` and `make dev-z8002` are shortcuts for the `m20` and
`z8002-demo` drives.  The drives are generated, not source trees, and do not
depend on the host: mount any of them in `cpm8k-z8001` or `cpm8k-z8002`.
Automated regression and logical-media generation call the same staging
operation in temporary directories.

The package also declares the logical media formats on which its source
overlay can be supplied:

```sh
make media-formats NAME=m20
make media NAME=m20 FORMAT=m20-hd

make media-formats NAME=m20-serial
make media NAME=m20-serial FORMAT=m20-hd

make media-formats NAME=z8002-demo
make media NAME=z8002-demo FORMAT=z8002-demo-hd

make media-formats NAME=plasmo
make media NAME=plasmo FORMAT=plasmo-hd
```

Media generation places the common development tree and the selected BIOS
overlay in a CP/M filesystem image.  It does not install `cpm.sys`, write a
boot sector, or run `putboot`; those are separate target-specific steps.

For the two Z8002 machines, a single target builds the system and a complete
bootable disk (system in LBA 0-127, filesystem from LBA 128):

```sh
make z8002-demo-image   # build/media/z8002-demo/z8002-demo-hd/
make plasmo-image       # build/media/plasmo/plasmo-hd/
```

## Creating a package

A new machine gets its own package directory, `src/bios/<name>`.  You decide
three things for it: its target CPU, the media format(s) its development tree
can be written to, and (at build time, not in the package) which host CPU runs
the build.

1. **Copy the closest package.**
   - `m20` -- a Z8001 board; empty overlay, add only what differs.
   - `z8002-demo` -- a Z8002 board with a multi-bank MMU.
   - `plasmo` -- a Z8002 board with a single bank window.
     [`plasmo/PORTING.md`](plasmo/PORTING.md) lists every change from
     `z8002-demo` and is a complete worked example.
2. **Set the target CPU and CCP+BDOS.**  Write `TARGET_CPU` and `CPMSYS`:
   `z8001` + `cpmsys.rel` (segmented), or `z8002` + `cpmsys2.rel`
   (non-segmented).  A Z8002 package also needs `cpmsys.sub` and `linksys.sub`
   overrides that link `cpmsys2.rel` (copy them from `z8002-demo`).
3. **Change the machine-specific code.**  Usually:
   - `biosio.8kn` -- console and disk ports and handshakes;
   - `bios.c` -- console defines, `memtab` (where the TPA is), `mem_bcp`
     (copies between the OS and the TPA);
   - `biosmem.8kn`, `biostrap.8kn` -- bank switching (`xfersc`);
   - `biosboot.8kn` -- stack and PSA addresses; the PSA must lie above the
     whole image including bss.
4. **Write the `Makefile`.**  Required targets:
   - `bios.rel` -- call `scripts/build-bios.sh` with `BIOS_OVERLAY`,
     `TARGET_CPU` and `HOST_CPU` (copy the rule from an existing package);
   - `media-formats` -- print the supported media format names.

   Optional: `system-artifacts`, which sysgen runs with `SYSTEMDIR` after
   linking -- e.g. a flat boot payload and a check that the image stays below
   the PSA (see `z8002-demo` or `plasmo`).
5. **Choose or add a media format.**  Reuse an existing one from `src/media/`,
   or add `src/media/<format>/format.conf` plus a cpmtools diskdefs file (see
   [`src/media/README.md`](../media/README.md)).  A bootable disk image
   (system + filesystem) needs its own script and `make` target, like
   `scripts/build-plasmo-hd.sh` / `make plasmo-image`.
6. **Build and test.**

   ```sh
   make system NAME=<name>                  # host = target CPU
   make system NAME=<name> HOST_CPU=z8001   # or pick the host explicitly
   make dev NAME=<name>                     # development drive
   make media NAME=<name> FORMAT=<format>
   ```

   The hosted emulator runs its own thin BIOS, not yours; running the new
   BIOS needs the real board or a machine emulator (e.g. a MAME driver).
