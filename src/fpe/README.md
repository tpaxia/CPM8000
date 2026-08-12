# Floating-point library sources

CP/M-8000 can emulate Z8000 extended floating-point instructions in software.
This directory holds the shared FPE core, its target-dependent support, and a
build script (`scripts/build-fpe.sh`) that assembles either Z8001 or Z8002
objects with the in-emulator toolchain (`asz8k` → `xcon`).

## What the two files are

- **`fpe.z8k`** — the emulator itself. It is the trap handler for the Z8000
  **EPA extended (floating-point) instructions**: when a program executes an
  `fld`/`fadd`/`fldctl`/… it takes an *extended-instruction trap*, and `fpe`
  decodes the two-word EPA/fpe encoding and emulates the operation. (The
  `epu:` routine is the decoder; per-op handlers are `Fadd:`, `Fld:`, …)
- **`fpedep.z8k`** and **`fpedep-z8002.z8k`** — system-dependent helpers
  (`gettext`/`getmem`/`putmem`) that map and copy operands between the trapped
  program's address space and the emulator's, using the `MEM_SC` memory-map
  system call.
- **`biosdefs.z8k`** and **`biosdefs-z8002.z8k`** — target definitions used
  while assembling the shared core and the corresponding helper.

## Source provenance and variants

There is far more than one copy of these sources floating around (the `newos`
and `bios` trees under `cpm_experiments/`, the M20 distribution binaries in
`src/cpm8k/`). They differ as follows.

### `fpe.z8k` — one parameterized core

The EPA-trap emulator logic is shared. Its saved-frame offsets are derived from
`co`, which is 4 bytes for a Z8001 call and 2 bytes for a Z8002 call.

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

The maintained Z8001 helper reconstructs the M20 distribution object. The
Z8002 helper uses the same memory-copy protocol but its one-word call return
address shifts the saved-register frame by two bytes. Although the Z8001 FPE
handler temporarily runs with segmented addressing disabled, Z8001 calls still
push segmented two-word return addresses; they are not Z8002 call frames.

The hosted Z8002 loader uses banked host storage for its merged logical address
space. FPE data mappings therefore retain the loader-selected user data bank;
they must not assume a fixed physical bank.

## Reproduction status

`fpedep.z8k` here was **transcribed from the distribution `fpedep.o`
disassembly** (the hand-optimized Z8001 variant above). Against
`biosdefs.z8k`, its machine code and relocations reproduce the distribution
object; the maintained object also exposes the parameter symbols `co` and
`pcbase`. `fpe.z8k` likewise reproduces the executable content of
`src/cpm8k/fpe.o` except for the uninitialized `epuwp` `.block` scratch noted
above, and carries the same additional parameter symbols.

Run `make fpe-regression` to build and execute the same floating-point test on
both hosted CPU targets.

The Z8002 object is also validated through the native Z8002-demo BIOS and MAME
machine. That path requires the BIOS to register `fp_epu` for `EPUTRAP` and to
preserve the current TPA data bank for nonzero map-4 operand requests. A
disposable MAME disk containing `FPTEST.Z8K` reports `FPTEST PASS` over the
machine's serial console.

## Bootstrap objects

`objects/z8001/` and `objects/z8002/` contain the target-specific x.out objects
used by the normal host build. This keeps `make emu` independent of a running
CP/M emulator. The objects are generated from the sources in this directory by
the original assembler running under the matching hosted CPU:

```sh
make regenerate-fpe       # replace the checked-in objects
make verify-fpe-objects   # rebuild in temporary directories and compare
```

The Z8001 and Z8002 objects are intentionally different because their call and
saved-PC frames differ. Verification assembles each variant under its matching
hosted CPU, which also catches staging of the wrong target definitions.
