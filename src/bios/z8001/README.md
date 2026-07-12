# CP/M-8000 BIOS for CPM8000 Board

This is essentially the [4sun5bu/Z8001MB](https://github.com/4sun5bu/Z8001MB)
CP/M-8000 BIOS (MIT license) — its BIOS dispatch and memory configuration
are taken directly from Z8001MB — with the system origin, I/O port
addresses, and serial/IDE configuration changed for the CPM8000 board
(i8251 UART, IDE disk, Z8530 SCC).

**This is not the original Olivetti M20 CP/M-8000 BIOS.** The M20 BIOS
sources (`.8kn` assembly and `bios.c`) can be found in `src/cpm8k/`
(extracted from the distribution disks).

See [CHANGES.md](CHANGES.md) for a detailed list of modifications from
the upstream Z8001MB project.
