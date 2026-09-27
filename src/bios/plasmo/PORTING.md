# Porting Z8002-demo to Plasmo: what changes

Start from a copy of `src/bios/z8002-demo`. Unchanged: `biosif.8kn`,
`biosdefs.8kn`, `syscall.8kn`, `biosboot.8kn` (code), `CPMSYS`
(`cpmsys2.rel`), `TARGET_CPU` (`z8002`).

## biosio.8kn

- Console: SIO `0FF1Bh`/`0FF1Fh` -> CPLD UART data `081h`, status `083h`.
  Tx-ready bit: bit 2 -> bit 1.
- Disk: ATA ports `0020h, 0025h ... 002Fh` -> CF ports `010h, 012h ... 01Eh`
  (data, error, count, LBA 7:0, 15:8, 23:16, device/head, status/command).
- Device/head byte: `40h` -> `0E0h`.

## bios.c

- Console defines: `RS232`/`KBD` `0xFF1B` -> `0x81`, `SERSTAT` `4` -> `2`,
  `SERXRDY` `0x04` -> `0x02`. Delete `SERCTRL`, `CTC_B`, the 8253 constants
  and `baudRates`.
- `serinit()`: empty (the UART has no control register).
- `memtab`: all 5 entries -> bank 1 (`0x01000000L, 0x10000L`). There is only
  one TPA.
- `mem_bcp()`: replace the 16 KiB chunk-3 window (`map_wdw`/`blkmov`/`unwdw`)
  with 256-byte chunks through `wbounce`, using `win_rd`/`win_wr`. A chunk
  never crosses a 32 KiB TPA half.

## biosmem.8kn

- Delete `_map_wdw`/`_unwdw` (special-I/O `0FFE3h`/`0FFE4h`).
- Add `_win_rd`/`_win_wr(half, off, buf, n)`: `outb` port `085h` = `half+2`,
  `ldirb` between `8000h+off` and `buf`, `outb` `085h` = `1`, `ret`. Use no
  stack while the window is open: it also covers the stack, the PSA and the
  code above `8000h`.
- Add `_wbounce: .block 256` in bss. `win_rd`, `win_wr` and `wbounce` must
  link below `8000h` (they do: `bios.rel` links first).

## biostrap.8kn

- `xfersc`: delete the `NBANK_I`/`NBANK_D` writes (`soutb 0FFE0h`/`0FFE1h`).
  Normal mode is a fixed 64 KiB on Plasmo.

## Package files

- `Makefile`: drop the monitor ROM install (`z8kmon.bin`); keep the flat
  `plasmo.boot` payload and the check that the image stays below `SYSPSA`
  (`0xB000`).
- Delete `mmu.v` and `z8kmon.bin`.
- `cpmsys.sub`/`linksys.sub`: comments only.

## Outside the package

- `src/diskdefs_plasmo`, `src/media/plasmo-hd/format.conf`: CF disk format.
- `scripts/build-plasmo-hd.sh` and `make plasmo-image`: system + disk image.
- MAME: `plasmo` driver in the `z8002-plasmo` branch of `tpaxia/mame`
  (see `PLASMO_TESTING.md`).

## Limit

Split-I/D programs (`0xEE0B`: zcc1/2/3, asz8k, ld8k) cannot run on Plasmo:
there is no second TPA bank.

The Zilog C compiler, assembler and linker need separate 64 KiB spaces for
code and data, but Plasmo gives a program only one; the board has enough RAM
for this (256 KiB fitted, only 128 KiB used today), so an updated CPLD that
addresses all of it and sends a program's instruction fetches and data
accesses to different banks would let the compiler run natively.
