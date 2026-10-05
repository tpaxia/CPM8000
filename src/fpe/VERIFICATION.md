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

## Defect

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

## Not covered

- `fremstep` is a partial-remainder step, not a full remainder, so it has no
  direct TestFloat counterpart.
- Extended precision, `fldbcd`, and the memory-operand addressing modes.
- The Z8002 target (all runs were Z8001).
- Larger vector sets (`testfloat_gen -level 2`).
