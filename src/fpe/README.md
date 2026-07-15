# Floating-point library sources

The Olivetti M20 (Z8001) has no Z8070 arithmetic coprocessor, so floating point
is done entirely in **software**. This directory holds the source for that
library and a build script (`scripts/build-fpe.sh`) that assembles it with the
in-emulator toolchain (`asz8k` → `xcon`).

## What the two files are

- **`fpe.z8k`** — the emulator itself. It is the trap handler for the Z8000
  **EPA extended (floating-point) instructions**: when a program executes an
  `fld`/`fadd`/`fldctl`/… it takes an *extended-instruction trap*, and `fpe`
  decodes the two-word EPA/fpe encoding and emulates the operation. (The
  `epu:` routine is the decoder; per-op handlers are `Fadd:`, `Fld:`, …)
- **`fpedep.z8k`** — the **system-dependent** half: small helpers
  (`gettext`/`getmem`/`putmem`) that map and copy operands between the trapped
  program's address space and the emulator's, using the `MEM_SC` memory-map
  system call.
- **`biosdefs.z8k`** — the shared definitions both halves `.input`.

## Source provenance and variants

There is far more than one copy of these sources floating around (the `newos`
and `bios` trees under `cpm_experiments/`, the M20 distribution binaries in
`src/cpm8k/`). They differ as follows.

### `fpe.z8k` — one universal version

The 4283-line EPA-trap emulator is **byte-identical in every tree**
(md5 `a3cb0e9…`, 101569 bytes). There is no source variation at all.

The only difference in the *built* `fpe.o` is the `epuwp` work area, which is
declared entirely with `.block` (reserved, uninitialized). The distribution
binary happens to hold leftover buffer garbage there; a clean build emits
zeros. No relocations touch that region and the emulator overwrites it at
startup, so the two are functionally identical.

### `fpedep.z8k` — three real variants

This is where all the divergence lives. The variants differ along three axes —
target CPU / include, instruction encoding, and which memory maps they perform:

| axis | `newos` (3654 B) | `bios` (3563 B) | **M20 distribution** (`src/fpe`) |
|------|------------------|-----------------|----------------------------------|
| **Target / include** | Z8002 — `biosdefs2.z8k` | Z8002 — `biosdefs2.z8k` | **Z8001 — `biosdefs.z8k`** |
| **Encoding** | `ld r5,#N` / `ldl rr2,#0` (unoptimized) | same | **`ldk r5,#N` / `xor r2,r2` / `clrb`** (optimized) |
| **`MEM_SC` map blocks** | both active (8 `sc`) | **2 commented out** (6 `sc`) | active, M20 layout (`gettext` has an extra `r5=#5` map) |
| **Map parameters** | `r5=#4` (TPA data) | note `r5=#2` (system data) | `r5=#5`/`#4`/`#0` (M20-specific) |

- **`newos`** and **`bios`** are the same lineage (Z8002, unoptimized `ld`/`ldl`),
  differing only in that `bios` **comments out two `MEM_SC` map calls** — a board
  where those maps aren't needed.
- The **M20 distribution** `fpedep` is a *separate, hand-optimized Z8001*
  version. It uses the short `ldk`/`xor`/`clrb` encodings, keeps all the maps
  (with the extra one in `gettext`), and — decisively — is built against the
  **segmented `biosdefs.z8k`**.

### Why `biosdefs.z8k`, not `biosdefs2.z8k`

`biosdefs.z8k` is the **Z8001 (segmented)** definitions; `biosdefs2.z8k` is the
**Z8002 (non-segmented)** variant. They differ in four things, the substantive
one being the saved trap frame:

- **`scseg`** — on the segmented Z8001 the frame carries a **PC-segment word**,
  so `biosdefs.z8k` inserts `scseg` (`caller PC SEG`) between `scfcw` and
  `scpc`; the frame is one word larger and every offset past it shifts.
  `biosdefs2.z8k` (non-segmented) drops it.
- `nr14` (segmented) vs `cr14` (non-seg) frame-slot naming.
- `biosdefs2.z8k` adds `co .equ 2` (`call offset, for 8002`).

The M20 is a Z8001, so the distribution built **both** fpe halves against the
segmented `biosdefs.z8k`. Assembling `fpedep` against `biosdefs2.z8k` gives the
wrong frame offsets *and* a divergent symbol table.

## Reproduction status

`fpedep.z8k` here was **transcribed from the distribution `fpedep.o`
disassembly** (the hand-optimized Z8001 variant above), so against
`biosdefs.z8k` it reproduces `src/cpm8k/fpedep.o`'s **object content
byte-for-byte** (only trailing file padding differs). `fpe.z8k` reproduces
`src/cpm8k/fpe.o` except the uninitialized `epuwp` `.block` scratch noted above.
