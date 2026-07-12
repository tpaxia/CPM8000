# BIOS Changes from 4sun5bu/Z8001MB

This BIOS is derived from [4sun5bu/Z8001MB](https://github.com/4sun5bu/Z8001MB)
(MIT license). The following changes adapt it to the CPM8000 board.
This is **not** the Olivetti M20 BIOS. The original M20 BIOS source files
(`.8kn` assembly and `bios.c`) can be found in `src/cpm8k/`.

A unified diff of all changes is in `z8001mb-to-m20.patch`.

## Summary of changes from Z8001MB to CPM8000

| What | Z8001MB (original) | CPM8000 (this BIOS) |
|---|---|---|
| System origin | `0x30000` | `0xb0000` |
| Z8530 SCC ports | `0x0005`/`0x0007` | `0x00C3`/`0x00C1` |
| Serial config | 4800 bps, 6 MHz, x16 clock | 115200 bps, 7 MHz, x64 clock |
| IDE ports | `0x002x` range | `0x01Cx` range |
| IDE init | Basic | Adds Set Features `0x82` (disable write cache) |
| IDE read/write | DRQ polling | Fixed 512-byte `djnz` loops |
| Memory regions | 5 (includes DDT) | 4 (DDT region removed) |
| New driver | — | `i8251.s` (alternative UART) |

## Unchanged files

- `bios.s` — BIOS main dispatch
- `bioscall1.s` — func0 (init), func1 (warm boot)
- `bioscall2.s` — func2-7 (console/char I/O)
- `bioscall3.s` — func8-14,16,21 (disk operations)
- `biosmem.s` — memory copy/mapping
- `put.s` — console output helpers (putln, putsp, puts, puthex)

## New files

### i8251.s
Alternative serial driver using i8251-style UART at ports `0xC3`/`0xC1`
with 8253 timer at `0xC7`. Replaces z8530.s for boards without a Z8530 SCC.

## Modified files

### cpmsys.x — Linker script
- System origin address changed from `0x30000` to `0xb0000`.

### biosboot.s — Bootstrap
- System address constant changed from `0x03000000` to `0x0B000000`,
  matching the linker script change.

### biosdef.s — Definitions
- Whitespace only (tab after `.equ` removed, trailing blank line removed).

### biosif.s — BIOS interface
- Commented out `ei vi, nvi` (interrupts remain disabled at boot).
- Commented out `call scc_init` (serial init is handled elsewhere or
  deferred depending on the driver used).

### bioscall4.s — Memory region table
- Reduced memory region count from 5 to 4.
- Commented out Region 5 (`0x09000000`, DDT debug region).

### biostrap.s — Trap handlers
- Minor: changed `fp_epu` comment style from line comments (`!`) to
  block comment (`/* */`).

### z8530.s — Z8530 SCC serial driver
- Port addresses changed from `0x0005`/`0x0007` to `0x00C3`/`0x00C1`.
- Initialization sequence rewritten:
  - Was: baud rate generator setup for 4800 bps at 6 MHz with x16 clock.
  - Now: direct-clocked 115200 baud at 7 MHz with x64 clock divider,
    no baud rate generator, added overrun reset.

### diskio.s — IDE disk I/O
- IDE port addresses changed from `0x002x` range to `0x01Cx` range.
- Added `CMDRDY` and `DATRDY` macros for polling IDE status.
- `disk_init` now sends an additional Set Features command (`0x82`)
  to disable write cache.
- Sector read/write loops changed from DRQ status-bit polling to
  fixed 512-byte counted loops using `djnz`.
