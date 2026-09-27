# Plasmo CP/M-8000 BIOS — Banking TPA support

How this BIOS runs transient programs (PIP, STAT, ED, user `.z8k` files) on
the **Plasmo** pseudo-segment banking register. Read this before changing
the memory-management code or this BIOS — several pieces here only make
sense together, and this hardware is considerably simpler (and more
constrained) than Z8002-demo's chunk-based MMU.

## Hardware source

This machine is a real, physical homebrew Z8002 board (not an emulated
MAME machine), documented by its designer ("Plasmo") on the
[VCFED "A homebrew Z8002" forum thread](https://forum.vcfed.org/index.php?threads/a-homebrew-z8002.1253969/).
The facts below come from the actual CPLD schematic
(`top_cpld_scm_segment_regs.pdf`, dated 2025-10-04), the memory-map diagram
(`Z8002 MemMap.pdf`), and the board's own monitor source (`Z8002mon.txt`,
v0.4a, 2025-09-13) — not from forum prose, which turned out to contain
errors when checked against the primary sources.

## The memory model

Total RAM is 128 KiB, in four fixed 32 KiB quarters:

| Region | Address | Mode | Fixed or mapped |
|---|---|---|---|
| System-Lo | `$0000-$7FFF` | System | **fixed** — identity, always the OS |
| System-Hi | `$8000-$FFFF` | System | **mapped** — selected by port `$85` |
| Normal-Lo | `$0000-$7FFF` | Normal | fixed — TPA, lower half |
| Normal-Hi | `$8000-$FFFF` | Normal | fixed — TPA, upper half |

Port `$85` is a 2-bit latch that affects **only** System-mode accesses to
`$8000-$FFFF`:

| `$85` | Maps System-Hi to |
|---|---|
| `00` | System-Lo (its own fixed region — "not useful but can be done") |
| `01` | dedicated System-Hi RAM — **reset/idle default** |
| `10` | Normal-Lo (TPA lower 32K) |
| `11` | Normal-Hi (TPA upper 32K) |

Crucially: **Normal mode is never banked.** Normal-Lo and Normal-Hi are
*always* the same fixed physical 64 KiB — there is no register that points
Normal mode anywhere else. This is the single biggest difference from
Z8002-demo's MMU, and it drives everything else below.

### Consequence: only one TPA, no split-I/D

Z8002-demo can hand a transient program a full 64K **code** bank and a
separate full 64K **data** bank (`NBANK_I`/`NBANK_D`), which is what lets it
run the Zilog C compiler chain (zcc1/zcc2/zcc3, asz8k, ld8k — all built as
oversized split-I/D objects, `0xEE0B`, because they don't fit one 64K bank).

Plasmo has exactly one 64 KiB TPA, period — Normal mode can't be pointed at
a second bank. **Split-I/D programs cannot run on this hardware.** Only
merged programs (single 64K, `0xEE03` — PIP, STAT, ED, WUMP, TICTAC, and
ordinary user `.z8k` files) are supported. Do not attempt to load the
compiler chain's split objects on Plasmo; there is nowhere for the data
bank to go, and `_usrdseg` would silently alias back onto the code bank.

`_usrseg`/`_usrdseg` are always `0x0100` (bank 1) in practice — there is
only one possible TPA to point them at.

### The System-Hi aperture (the copy/map window)

To reach the TPA from system mode (the BDOS/loader moving an FCB, DMA
buffer, or record between OS space and the TPA), the OS temporarily
reprograms port `$85` to point System-Hi at whichever TPA half it needs,
does the transfer through `$8000-$FFFF`, then restores `$85 = 01` (its
normal, idle state — System-Hi's own dedicated RAM). This plays exactly the
role Z8002-demo's chunk-3/`SAP0` aperture plays, just sized to 32 KiB
(one TPA half) instead of 16 KiB (one chunk), and switched with a single
`OUT $85` instead of two special-I/O writes.

**Hard constraint, unlike Z8002-demo:** Plasmo's bank mux does not
distinguish instruction fetches from data accesses — *any* System-mode
access to `$8000-$FFFF` is redirected while `$85` is diverted. That makes
everything the OS keeps in System-Hi unreachable while the window is open:
instruction fetches, the system stack (`SYSSTK` `0xBF00`), the PSA
(`0xB000`), and any OS buffer above `$8000` (`trkbuf` straddles it).

So windowed copies happen only in two leaf routines, `win_rd`/`win_wr`
(`biosmem.8kn`): they load all arguments into registers, open the window,
`ldirb` between the window and `wbounce` (a 256-byte buffer in the BIOS's
own bss), close the window, and only then `ret`. No stack is used while
the window is open, and no interrupts are wired on Plasmo. `mem_bcp` moves
the OS side of each chunk to/from `wbounce` with the window closed. This
relies on `win_rd`, `win_wr` and `wbounce` being linked below `$8000`,
which link order gives: `bios.rel` links first, at `$0000`
(`scripts/linkcpmsys.sub`). Currently `win_rd`/`win_wr` are at
`0x024e`/`0x0276` and `wbounce` at `0x72cc` (`build/tools/xoutdump
build/system/plasmo/cpm.sys`).

## Placing and reaching the TPA

- `memtab` (`bios.c`, BIOS function 18 GMRTA) gives the loader the TPA
  region: just one, bank 1, the full 64K. The entries other machines use
  for a second/third bank are aliased to bank 1 (see the comment in
  `bios.c`) purely to keep the table's shape what the loader/BDOS expect;
  they are otherwise inert.
- `map_1` (`biosmem.8kn`) is the same generic pseudo-segment dispatch every
  Z8002 target uses — unchanged from Z8002-demo, because it doesn't depend
  on the physical MMU, only on the OS-vs-TPA addressing model.
- **`mem_bcp` (`bios.c`)** is the cross-bank block copy the BDOS/loader use.
  Bank-0-to-bank-0 copies are a plain `blkmov`. Anything touching the TPA
  goes in chunks of at most 256 bytes (never crossing a TPA half) through
  `wbounce` and `win_rd`/`win_wr`, which write `half+2` to port `$85`
  (`2`=Normal-Lo, `3`=Normal-Hi) and restore `$85 = 1`.
- **`xfersc` (`biostrap.8kn`)** does *not* program any bank register before
  `IRET`-ing into a transient — there is nothing to program. Normal mode
  always sees the one physical TPA.

## The BDOS_SC caller-bank injection

Same mechanism and same reason as Z8002-demo (see the long comment in
`biostrap.8kn`): a transient in Normal mode passes the BDOS a bare 16-bit
FCB/DMA offset, and the stock `cpmsys2.rel` was built flat, so the BIOS
tags a Normal-mode BDOS caller's `rr6` high word with `_usrdseg` (always
`0x0100` here) so `mem_bcp` can find it. System-mode callers (CCP, the
loader) are left untouched, for the same reasons as Z8002-demo.

## Floating-point trap and data mapping

Unchanged from Z8002-demo: `trapinit` installs `fp_epu`, and the FPE uses
`MEM_SC` map 4 for operand accesses in the TPA data space — which, again,
is always the same single 64K bank here.

## File map

| File | Role |
|------|------|
| `biosboot.8kn` | boot transfer vector at `$0000`, PSA/stack addresses |
| `bios.c`       | `memtab` (single TPA bank), `mem_bcp` (aperture copy), console/disk glue |
| `biosio.8kn`   | CPLD UART console (`$81`/`$83`), CF ATA task-file (`$10-$1E`) |
| `biosmem.8kn`  | `map_1`, `win_rd`/`win_wr` + `wbounce` (port `$85` aperture), `blkmov`, `memsc` |
| `biostrap.8kn` | trap dispatch, BDOS_SC caller-bank injection, `xfersc` (no-op bank switch) |
| `syscall.8kn`  | C-callable SC wrappers (`_bdos`, `_bios`, `_map_adr`, `_xfer`) — generic, unchanged |

## Status and open items

The BIOS source in this package is a from-scratch adaptation of the
Z8002-demo BIOS to Plasmo's real, verified hardware facts (memory map, I/O
ports, the erratum-safe register-indirect I/O convention). It builds cleanly
via the in-emulator DR/Zilog toolchain (`make system NAME=plasmo`).

Under the MAME driver (`plasmo` in the `z8002-plasmo` branch of `tpaxia/mame`, see
`PLASMO_TESTING.md`) with the CF image from
`make plasmo-image`, the following work: boot to `A>`, `dir` (full
listing of the 118-file development disk), `type`, `stat` (a transient
loaded into the TPA, run in Normal mode, returning to the CCP), and
`pip copy.sub=wump.sub` followed by `type copy.sub` / `dir copy.sub`
(disk write path).

Two BIOS bugs had to be fixed to get there:

1. **PSA inside the bss.** `SYSPSA` was `0x9300`, copied from Z8002-demo,
   but the image's bss runs to about `0x9400`. `trapinit` writes the trap
   entries at `SYSPSA+4..+0x17`, which landed inside the CCP's `_dma`
   buffer; `dir` copies directory records into `_dma`, overwriting the
   system-call trap vector, so the next `sc` went astray and the scan
   restarted forever. `SYSPSA` is now `0xB000`, and the Makefile fails the
   build if `cpm.flat` (which includes bss) reaches it.
2. **Stack use while System-Hi was windowed.** The old `mem_bcp` called
   `map_wdw`/`blkmov`/`unwdw` from C with the window open, so calls and
   returns went through a stack that was, at that moment, TPA memory, and
   OS buffers above `$8000` were unreachable. Every transient hung on
   load. Now done by `win_rd`/`win_wr` as described above.

The Z8002-demo target had the same `SYSPSA 0x9300` value, with its trap
entries inside the CCP's `_save_su` (SUBMIT save area). It has been moved to
`0xB000` there as well, with the same Makefile check.

Two things remain separately open, unrelated to the above:

1. **No compiled monitor/ROM binary for the real board.** The board's own
   monitor (`Z8002mon.txt`, 2025-09-13) predates the pseudo-segment banking
   scheme entirely and has no command that loads a full 64K system image
   and jumps to it — its one app-loading path (`B` command, "User Apps")
   only loads `$0000-$AFFF` (44 KB) and keeps the monitor resident at
   `$B000-$BFFF`, which isn't enough room for this ~37 KB+ system plus
   stack. Getting a system image onto real hardware needs either a monitor
   extension (a new command that loads the full image and jumps to
   `$0000`, matching this BIOS's transfer-vector convention) or another
   loading path — that's separate work from this BIOS.
2. **The MAME driver's boot path is a test-harness shortcut, not a model of
   real hardware.** It uses MAME's `quickload` mechanism to inject the
   system image directly into RAM rather than modeling the real (undumped)
   CPLD bootstrap ROM — see the driver's header comment and
   `PLASMO_TESTING.md` for what that means and doesn't mean.
