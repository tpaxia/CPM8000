# Floating-point library sources

CP/M-8000 can emulate Z8000 extended floating-point instructions in software.
This directory holds the shared FPE core, its target-dependent support, and a
build script (`scripts/build-fpe.sh`) that assembles either Z8001 or Z8002
x.out objects with the original in-guest toolchain. `ASZ8K` invokes `XCON` as
an implicit second pass, so both programs are staged even though `fpe.sub`
names only `ASZ8K`.

## Files

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

## Target variants

### Shared core

The EPA-trap emulator logic is shared. Its saved-frame offsets are derived from
`co`, which is 4 bytes for a Z8001 call and 2 bytes for a Z8002 call.

When comparing the maintained **Z8001** build with the original M20 Z8001
`src/cpm8k/fpe.o`, the only executable-content difference is the `epuwp` work
area, which is declared entirely with `.block` (reserved, uninitialized). The
distribution object happens to hold leftover buffer data there; a clean build
emits zeros. No relocations touch that region and the emulator overwrites it at
startup. The Z8002 object is separately built and intentionally differs because
it uses the Z8002 frame constants described below.

### Target-dependent support

`fpedep.z8k` and `biosdefs.z8k` implement the segmented Z8001/M20 selection.
`fpedep-z8002.z8k` and `biosdefs-z8002.z8k` implement the non-segmented Z8002
selection. The substantive differences are:

- **`scseg`** — on the segmented Z8001 the frame carries a **PC-segment word**,
  so `biosdefs.z8k` inserts `scseg` (`caller PC SEG`) between `scfcw` and
  `scpc`; the frame is one word larger and every offset past it shifts.
  `biosdefs-z8002.z8k` (non-segmented) drops it.
- `nr14` (segmented) vs `cr14` (non-seg) frame-slot naming.
- The call-return offset `co` is 4 for Z8001 and 2 for Z8002.
- The helpers select the `MEM_SC` address spaces appropriate to each target's
  memory model.

The maintained Z8001 helper reconstructs the M20 distribution object. The
Z8002 helper uses the same memory-copy protocol but its one-word call return
address shifts the saved-register frame by two bytes. Although the Z8001 FPE
handler temporarily runs with segmented addressing disabled, Z8001 calls still
push segmented two-word return addresses; they are not Z8002 call frames.

The hosted Z8002 loader uses banked host storage for its merged logical address
space. FPE data mappings therefore retain the loader-selected user data bank;
they must not assume a fixed physical bank.

## Provenance and reproduction

The original M20 distribution objects are retained as `src/cpm8k/fpe.o` and
`src/cpm8k/fpedep.o`. The maintained Z8001 `fpedep.z8k` was transcribed from
the distribution `fpedep.o` disassembly. Against
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
saved-PC frames differ. `SUBMIT FPE` is common to both targets; the selected
development drive supplies the matching helper and definitions. Verification
assembles each variant under its matching hosted CPU, which also catches
staging of the wrong target definitions.
