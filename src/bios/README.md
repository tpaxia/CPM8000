# BIOS packages

The directories below this one are source-overlay packages used by `sysgen`.
The build first stages the common M20 CP/M-8000 BIOS sources from `src/cpm8k`,
then applies the selected package's `.c` and `.8kn` files.  A package therefore
contains only the files that differ from the common source tree.

The current packages are:

| Package | Console | Purpose |
|---------|---------|---------|
| `m20` | M20 keyboard and video display | Builds the stock M20 BIOS without source overrides. |
| `m20-serial` | M20 RS-232 terminal port at 9600 baud | Provides a serial console suitable for a host PTY, scripted sessions, and headless testing. |

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
```

The resulting files are written to:

```text
build/bios/<name>/bios.rel
build/system/<name>/cpm.sys
```

Each package implements this build contract:

```sh
make -C src/bios/<name> bios.rel BUILDDIR=<directory>
```

## Development media

The package also declares the logical media formats on which its source
overlay can be supplied:

```sh
make media-formats NAME=m20
make media NAME=m20 FORMAT=m20-hd

make media-formats NAME=m20-serial
make media NAME=m20-serial FORMAT=m20-hd
```

Media generation places the common development tree and the selected BIOS
overlay in a CP/M filesystem image.  It does not install `cpm.sys`, write a
boot sector, or run `putboot`; those are separate target-specific steps.
