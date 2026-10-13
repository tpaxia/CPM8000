# Verifying the floating-point emulator

The FPE (`fpe.z8k`) is checked against Berkeley TestFloat, an independent
IEEE 754 reference. The harness lives in `tests/fpe/`; this file records how
it works and what it found. No FPE source was changed to produce these results.

## Method

`tests/fpe/fpevec.py` runs `testfloat_gen` for one operation and rounding
mode, packs the vectors into `VEC.BIN`, and runs `FPVEC.C` under the hosted
CP/M-8000 emulator (Z8001). The guest executes the EPA instruction for each
vector and prints a `BAD` line for every difference in result bits or
exception flags; the host script classifies and summarizes them.

- `tests/fpe/genops.py` generates `fpops.8kn`, the assembly stubs. Each stub
  loads its operands, clears the sticky flags, executes one instruction, stores
  the result, then reads the flags (after the store, see below).
- The assembler only encodes extended-precision register operations, so the
  arithmetic stubs emit the instruction words raw, with the rounding-precision
  field (word 2, bits 13-12) set explicitly: 0 extended, 1 single, 2 double.
- Vectors are TestFloat level 1 (about 46k per binary operation per mode;
  fewer for unary operations).

```sh
# testfloat_gen: build Berkeley SoftFloat 3 and TestFloat 3 (build/Linux-x86_64-GCC
# works on any POSIX host with gcc), then:
make emu
TESTFLOAT_GEN=/path/to/testfloat_gen tests/fpe/fpevec.py [-r MODE] [-a] [-b] OP...
```

Operations: `s_`/`d_` (binary32/binary64) `add sub mul div sqrt eq le lt`,
`i32_s i32_d s_i32 d_i32 s_d d_s`, and `s_int d_int` (roundToInt). Rounding
modes: `near_even minMag min max`. `-a` selects affine infinity mode, `-b`
expects underflow tininess detected before rounding.

## Result summary

Arithmetic (`add sub mul div sqrt`, single and double, all four rounding
modes), conversions and comparisons match TestFloat on every vector except
for the differences listed under "Expected differences", and the one defect
below.

## Defects

### `fint` with double rounding precision is wrong

`d_int` (roundToInt, rounding-precision field = double): about 200 of 768
vectors fail. The same operation with the extended field matches TestFloat,
and single precision (`s_int`) matches.

| input (hex)          | value      | got | expected |
|----------------------|------------|-----|----------|
| `3FE81C15C02BA183`   | 0.75       | 0   | 1        |
| `401FFFF000000004`   | 7.99999…   | 0   | 8        |
| `C02ECA00AD8F3EDA`   | -15.8      | -0  | -15      |
| `434BDEA404BD39C8`   | ~2^52 int  | `…04BD3800`, inexact | unchanged, exact |

Failures are mostly results that round up or carry, plus values near and
above 2^52 whose low bits are cleared and flagged inexact although the input
was already an integer. `Fint` (`fpe.z8k`, `intno`) selects the integer
position 0x4033 (bias + 52) for double and calls `shiftdigs` and `round`;
those are the first places to look. The cause has not been traced. Whether
real programs reach it depends on whether the compiler ever emits `fint`
with the double precision field (not checked).

Reproduce: `tests/fpe/fpevec.py d_int`.

### `fldbcd` loses a carry

Found by reading the code against the copy of this emulator in the ZEUS 3.21
kernel (see "Comparison with the ZEUS kernel's copy" below), not by a test
run.

`Flbcd11` accumulates the decimal digits into the 64-bit integer in
`r8`-`r11`: it multiplies the accumulator by ten with `addl`/`adc` chains,
which carry correctly, and then adds the next digit with

```
	add	r11,r0
	djnz	r12,Flbcd11
```

The carry out of `r11` is not propagated to `r10`, `r9` and `r8`. Whenever
adding a digit makes the low word wrap, 65536 is lost from the value. A model
of the loop gives the first wrong inputs as 65536 to 65539 (loaded as 0 to
3) and 131072 to 131075 (loaded as 65536 to 65539); any longer number whose
leading digits pass through such a value is wrong as well.

The ZEUS kernel's copy has the correction:

```
	sub	r4,r4
	add	r11,r0
	adc	r10,r4
	adc	r9,r4
	adc	r8,r4
	djnz	r12,Flbcd11
```

(`r4` is free at that point: the saved copy of the accumulator in `rr4` has
already been used.) The same model with the carry propagated is correct for
every value tried.

Every copy in this repository has the uncorrected form: `src/fpe/fpe.z8k`,
the distribution object `src/cpm8k/fpe.o`, the objects under
`src/fpe/objects/`, and the `fpe` code on the distribution disk images. The
source is left as it is, because it reproduces the distribution object.

`fldbcd` has not been run under `tests/fpe/` (see "Not covered"), so this
defect is established from the code and the model only.

## Expected differences

These come from the draft standard the FPE implements (1980-82 sources) and
are not defects.

- **Projective infinity mode.** After reset the FPE is in projective mode:
  Inf+Inf, Inf-(-Inf) and sqrt(+Inf) return a NaN with invalid set, and
  1 vs +Inf compares unordered. With affine mode (`-a`, mode bit 0) these
  match IEEE. `startup.o` leaves the FPE in the projective state.
- **Range coercion is off.** The single and double range checks in `under`
  and `over` are commented out ("RANGE COERCION"). Operations round to the
  format's precision but check range against the extended exponent range;
  the narrower range is enforced when the result is stored. Overflow of a
  single-precision add is therefore flagged at the store, not at the add.
- **Tiny results.** Underflow is flagged for any tiny result, including exact
  subnormals (IEEE flags it only when also inexact), and tininess is detected
  before rounding (`d_s` flag mismatches disappear with `-b`). Subnormal
  results are rounded to the format's precision and then denormalized at the
  store, so they can be rounded twice.
- **NaNs.** Payload, sign and signaling behavior differ. Operands are quieted
  when loaded into an EPU register, so signaling-NaN inputs never raise
  invalid: comparisons with a NaN operand return the correct result but
  without TestFloat's invalid flag. Whether `le`/`lt` with a quiet NaN should
  signal invalid (the signaling compare is `fcpx`) was not checked.
- **Float to int32, out of range or NaN.** The FPE sets integer overflow
  (0x80) instead of invalid and does not saturate (about 180 and 280 vectors
  for `s_i32` and `d_i32`). All in-range results match.
- **ZCC float arithmetic.** The compiler emits register operations with
  rounding-precision field 3, which the FPE treats as double. C `float`
  arithmetic is therefore computed in double and rounded to single at the
  store (double rounding is possible for single division and sqrt).

## Comparison with the ZEUS kernel's copy

ZEUS 3.21, the Unix of the Zilog System 8000, executes extended instructions
with the same Zilog emulator when the machine has no floating-point unit. Its
kernel image keeps the routine names, so the two can be compared routine by
routine. The comparison was made on instruction mnemonics, after setting
aside what follows from the kernel copy being a segmented build (long
pointers, so `ldl` for `ld`, and `calr` where the assembler could reach).

- 77 routines have the same name on both sides; 73 have the same instruction
  sequence (1,752 instructions in the kernel copy against 1,755 here).
  Constants were compared by hand in `under` and `resinv` only, and match.
- `Fldbcd` differs by the carry correction described above.
- `setup` and the dispatch in `epu` scale a table index by four instead of
  two, because the kernel's jump tables hold long addresses.
- The entry point differs. The kernel's (`fpepu`) is a C-callable function
  taking the saved-state frame and the work area, and it calls two hooks
  around the operation that are empty in that kernel. It also has three
  instructions near its end that were not analysed.
- `Fint`, including `intno`, is identical, so the double-precision `fint`
  defect above should be present in ZEUS too. This is inferred from the
  code; it was not run there.
- `Fremstep` is identical.
- The work area has the same layout.

The kernel image compared is `/zeus` from a ZEUS 3.21 installation; the
script is `abi/tools/fpecmp.py` in the Zeus_emulator project.

## Not covered

- `fremstep` is a partial-remainder step, not a full remainder, so it has no
  direct TestFloat counterpart.
- Extended precision, `fldbcd`, and the memory-operand addressing modes.
- The Z8002 target (all runs were Z8001).
- Larger vector sets (`testfloat_gen -level 2`).
