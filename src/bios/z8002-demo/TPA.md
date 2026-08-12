# Z8002 CP/M-8000 BIOS — Banking TPA support

How this BIOS runs transient programs (PIP, STAT, ED, user `.z8k` files) on the
**Z8002-demo** banking MMU. MAME is the primary machine implementation; the
compatible physical implementation and its RTL are documented under
`Z8000_FPGA/z8000_examples/cpm8000_z8002`. Read this before changing the MMU or
this BIOS—several pieces here only make sense together.

## The two-tier memory model (recap)

The MMU has two register sets, chosen by the CPU N/S (system/normal) status line:

- **System mode** (OS: CCP/BDOS/BIOS): 16 KB paging. Each of the four logical
  16 KB chunks C0..C3 is either **A** (identity into the `SHOME` bank) or **B**
  (a movable aperture, `SAP0`/`SAP1`). Registers: `SHOME`, `SSEL`, `SAP0`,
  `SAP1` at special-I/O `0xFFE2..0xFFE5`.
- **Normal mode** (a TPA program): whole-64 KB, offset-preserving. Instruction
  fetches use `NBANK_I`, data uses `NBANK_D` (independent 3-bit bank registers,
  `0xFFE0`/`0xFFE1`). Merged program: `NBANK_I == NBANK_D`. Split-I/D program:
  they differ (the loader fills each bank; the MMU routes I vs D by the `id_instr`
  line — no OS change needed, the hardware already supports it).

`SC` traps up from normal to system mode; `IRET` drops back. The mode switch
*is* the remap, so the OS never has to steal address space from the TPA.

## Bank-0 layout — why chunk 3 is kept free

The OS is ~37 KB and would otherwise fill all of bank 0, leaving nowhere to open
an aperture into a TPA bank. So `biosboot.8kn` puts the PSA and the system stack
in chunk 2's tail and keeps **chunk 3 (`0xC000-0xFFFF`) entirely free** as the
copy/map window:

```
0x0000-0x9252  code+data+bss   (chunks 0,1,2)
0x9300         PSA (trap vectors)
..0xBF00       system stack (grows down, ~11 K)   SYSSTK
0xC000-0xFFFF  aperture window (chunk 3)          -- NOT used by the OS
```

## Placing and reaching the TPA

- `memtab` (bios.c, BIOS function 18 GMRTA) gives the loader the TPA regions. A
  pseudo-segment's **high byte is the physical bank number** (bank N <-> `N<<8`;
  the OS is bank 0). Merged TPA = bank 1; split-I/D data = bank 2.
- `map_1` (biosmem.8kn) returns the TPA **code** bank `_usrseg` for `usr_prog`
  (TPAPROG) and the TPA **data** bank `_usrdseg` for `usr_data`/`call_data`;
  OS space returns `_sysseg=0`. `_usrseg` is set to the code bank by the
  loader's `map_adr(region,-1)`.
- **`mem_bcp` (bios.c)** is the cross-bank block copy the BDOS/loader use. It
  addresses bank 0 directly and any TPA bank through the C3 aperture, clipping
  each `blkmov` to a 16 KB chunk. `map_wdw(physchunk)` (biosmem.8kn) points
  `SAP0` at the wanted physical chunk and sets `SSEL=0x08` (C3=B, C0-C2=A). With
  chunks 0-2 = A they stay identity-mapped to bank 0, and C3 is the lowest B
  chunk so it binds to `SAP0` (mmu.v). `unwdw()` restores `SSEL=0`.
- **`xfersc` (biostrap.8kn)** sets `NBANK_I=_usrseg` (code) and `NBANK_D=_usrdseg`
  (data) before `IRET`-ing into the transient's normal-mode context.

### Merged vs split-I/D

A merged program (`0xEE03`, e.g. PIP) keeps code and data in ONE bank:
`_usrdseg == _usrseg` and `NBANK_I == NBANK_D`.

A split-I/D program (`0xEE0B`, e.g. the compiler passes zcc1/2/3, asz8k, ed --
too big to fit one 64 KB bank) puts code in the I-bank and data/bss/stack in a
separate D-bank, so it gets a full 64 KB of each. The loader signals this by
calling `map_adr(0,TPADATA)` **only** for split programs; the BIOS's `usr_data`
then derives the data bank as **code bank + 1** (matching `memtab`: SPREG bank 1,
SDREG bank 2) and records it in `_usrdseg`. That one variable is what the FCB/DMA
injection and `NBANK_D` use, so both the load-time placement and the run-time
BDOS access land in the transient's data bank. (`mmu.v` routes I vs D by the
`id_instr` line; the OS's system-mode apertures are I/D-agnostic, so it writes a
physical chunk once and the transient later fetches or reads it in normal mode.)

## The BDOS_SC caller-bank injection (the subtle one)

This is the piece most likely to confuse a future reader, so it is spelled out
in a big comment in `biostrap.8kn` and again here.

**Problem.** A transient runs in normal mode in its TPA bank and calls the BDOS
with an FCB/DMA pointer. Being a **non-segmented** program it passes a bare
16-bit offset (`rr6 = 0:offset`) — it *cannot* know its own bank, because
`NBANK_*` are privileged special-I/O registers it may not read. But the BDOS,
running in bank 0, must reach that FCB/DMA in the TPA's bank, which it does with
`mem_bcp` using a `{bank, offset}` pointer (`traphnd`: `ldl rr2,rr6`). So the
high word of `rr6` has to carry the caller's bank.

**Why the BDOS doesn't do it.** The stock `cpmsys2.rel` was built for a **flat**
layout (OS and TPA sharing one 64 KB bank, where a bare offset already resolves),
so the tag `ld r2,_usrseg` is **commented out** in `bdosif2.z8k`. It is a build
choice, not a bug.

**What we do.** Rather than rebuild the BDOS, the **BIOS** injects the bank in
`trap_disp` (biostrap.8kn). For a BDOS system call (`sc #2`) from a **NORMAL-mode**
caller it sets `rr6`'s high word to `_usrdseg` — the transient's **data** bank
(FCB/DMA are data; `== _usrseg` unless split-I/D).

**Only normal callers are tagged; a system-mode caller is left untouched** — and
that is essential. The CCP passes bank-0 pointers, and the program LOADER runs
inside the BDOS in system mode and passes REAL bank-1 TPA pointers (it points the
DMA into the TPA with `set DMA`, fn 26). Forcing `r6=0` there would send the read
into bank 0 and the TPA would never load. So: system callers carry their own bank
(via their own `map_adr`); only the non-segmented transient, which cannot, is
injected.

**If you change things:**
- Make the TPA share bank 0 (flat) -> `_usrseg`/`_usrdseg` become 0, injection is inert.
- Rebuild the BDOS with the `_usrseg`/`_usrdseg` tag enabled -> remove the injection so the
  pointer isn't tagged twice.

## File map

| File | Role |
|------|------|
| `biosboot.8kn` | bank-0 layout (SYSSTK/SYSPSA), keeps chunk 3 free |
| `bios.c`       | `memtab` (banks), `mem_bcp` (cross-bank copy), disk/console |
| `biosmem.8kn`  | `map_1`, `map_wdw`/`unwdw` (C3 aperture), `blkmov`, `memsc` |
| `biostrap.8kn` | trap dispatch, **BDOS_SC caller-bank injection**, `xfersc` (NBANK) |
| `syscall.8kn`  | C-callable SC wrappers (`_bdos`, `_bios`, `_map_adr`, `_xfer`) |

## Status

Working end to end: boot, console, `dir`, `type`, built-in writes (`ren`,
`era`), and **transient programs — both merged and split-I/D**.

- Merged: `PIP` loads cross-bank into its TPA bank, runs in normal mode, and does
  full file I/O through the BDOS (verified: `pip dst=src` is byte-identical to
  the source, read back off the disk image with cpmtools).
- Split-I/D: the **Zilog C compiler** runs — `zcc -c hello.c` chains the driver
  through zcc1/zcc2/zcc3 (each a split transient, up to ~54 KB of code, code in
  the I-bank and data in the D-bank) and emits a valid `HELLO.O` (x.out `0xEE02`
  object).
