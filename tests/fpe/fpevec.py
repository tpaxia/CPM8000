#!/usr/bin/env python3
"""Check the Z8000 floating-point emulator (src/fpe) against Berkeley TestFloat.

For one operation and rounding mode this generates vectors with
testfloat_gen, packs them into VEC.BIN, runs FPVEC.C under the hosted CP/M-8000
emulator, and summarizes the mismatches.

usage: fpevec.py [-t z8001|z8002] [-r MODE] [-n LIMIT] [-a] [-b] [-k] OP...
  OP    s_add s_sub s_mul s_div s_sqrt d_add d_sub d_mul d_div d_sqrt
        s_eq s_le s_lt d_eq d_le d_lt (comparisons, run in affine mode)
        i32_s i32_d s_i32 d_i32 s_d d_s s_int d_int (conversions;
        s_int/d_int are roundToInt), or "all"  (s = binary32, d = binary64)
  -r    near_even (default), minMag, min, max
  -n    run only the first LIMIT vectors
  -k    keep the staging directory

testfloat_gen is found through $TESTFLOAT_GEN or $PATH; build it from
https://github.com/ucb-bar/berkeley-testfloat-3 (Linux-x86_64-GCC works on
any POSIX host with gcc).  Run `make emu` first.

TestFloat expects IEEE 754-2008 semantics; the FPE implements an earlier
draft, so mismatches are sorted into:
  value  the result bits differ (a real defect unless the result is tiny)
  flags  only the exception flags differ
  tiny   the result is subnormal (the FPE rounds to the format's precision
         with extended exponent range and denormalizes again at the store,
         so tiny results can be rounded twice), or underflow was flagged on
         an exact subnormal result
  range  float-to-int32 where the result is out of range or a NaN: the FPE
         sets integer overflow (0x80) instead of invalid and may saturate
         differently
  nan    both results are NaNs (payload, sign and signaling behavior differ
         in the draft, and operands are quieted when loaded into the EPU)
Only value and flags mismatches make the exit status nonzero.
"""
import argparse, os, re, shutil, struct, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OPS = ["add", "sub", "mul", "div", "sqrt"]
NAMES = ["%s_%s" % (p, o) for p in "sd" for o in OPS]
# name -> (TestFloat function, operands, result kind, extra testfloat_gen args)
# result kinds: s single, d double, i int32, b bool
OPDEF = {}
for _p, _f in (("s", "f32"), ("d", "f64")):
    for _o in OPS:
        OPDEF["%s_%s" % (_p, _o)] = ("%s_%s" % (_f, _o), 1 if _o == "sqrt" else 2, _p, [])
for _p, _f in (("s", "f32"), ("d", "f64")):
    for _o in ("eq", "le", "lt"):
        n = "%s_%s" % (_p, _o)
        NAMES.append(n)
        OPDEF[n] = ("%s_%s" % (_f, _o), 2, "b", [])
for n, fn, kind, extra in (
        ("i32_s", "i32_to_f32", "s", []), ("i32_d", "i32_to_f64", "d", []),
        ("s_i32", "f32_to_i32", "i", ["-exact"]),
        ("d_i32", "f64_to_i32", "i", ["-exact"]),
        ("s_d", "f32_to_f64", "d", []), ("d_s", "f64_to_f32", "s", []),
        ("s_int", "f32_roundToInt", "s", ["-exact"]),
        ("d_int", "f64_roundToInt", "d", ["-exact"])):
    NAMES.append(n)
    OPDEF[n] = (fn, 1, kind, extra)

MODES = {"near_even": 0x00, "minMag": 0x04, "max": 0x08, "min": 0x0C}
# TestFloat flag bits -> FPE sticky-error byte bits
FLAGS = [(1, 0x10), (2, 0x04), (4, 0x02), (8, 0x08), (16, 0x01)]
SRC = ["zcc.z8k", "zcc1.z8k", "zcc2.z8k", "zcc3.z8k", "ld8k.z8k",
       "startup.o", "libcpm.a", "asz8k.z8k", "asz8k.pd", "xcon.z8k"]


def flags(tf):
    return sum(f for t, f in FLAGS if tf & t)


def build_vec(op, mode, limit, gen, affine=False, before=False):
    fn, arity, kind, extra = OPDEF[op]
    if kind == "b":
        affine = True    # TestFloat compares are affine
    cmd = [gen, "-r" + mode] + extra + [fn]
    if before:
        cmd.insert(1, "-tininessbefore")
    lines = subprocess.run(cmd, check=True, capture_output=True,
                           text=True).stdout.split("\n")
    lines = [l for l in lines if l]
    if limit:
        lines = lines[:limit]
    def val(x):
        v = int(x, 16)
        return (v >> 32, v & 0xFFFFFFFF) if len(x) > 8 else (v, 0)
    body = bytearray()
    for l in lines:
        f = l.split()
        if arity == 1:
            f = [f[0], "0"] + f[1:]
        a, b, z, fl = f
        body += struct.pack(">8L", *val(a), *val(b), *val(z),
                            flags(int(fl, 16)), 0)
    head = struct.pack(">8L", 0x46505631, NAMES.index(op), MODES[mode] | affine,
                       len(lines), 0, 0, 0, 0)
    return head + bytes(body), len(lines)


def run(op, mode, limit, target, gen, keep, affine=False, before=False):
    emu = os.path.join(ROOT, "build/emu/cpm8k-" + target)
    if not os.access(emu, os.X_OK):
        sys.exit("%s missing -- run 'make emu'" % emu)
    vec, n = build_vec(op, mode, limit, gen, affine, before)
    d = tempfile.mkdtemp(prefix="fpevec.")
    try:
        for f in SRC:
            shutil.copy(os.path.join(ROOT, "src/cpm8k", f), d)
        for f in ("FPVEC.C", "fpops.8kn"):
            shutil.copy(os.path.join(ROOT, "tests/fpe", f), d)
        os.rename(os.path.join(d, "fpops.8kn"), os.path.join(d, "FPOPS.8KN"))
        with open(os.path.join(d, "VEC.BIN"), "wb") as fh:
            fh.write(vec)
        script = ("ASZ8K -o FPOPS.O FPOPS.8KN\nZCC -C -M1 FPVEC.C\n"
                  "LD8K -W -S -O FPVEC.Z8K STARTUP.O FPVEC.O FPOPS.O -LCPM\n"
                  "FPVEC\nEXIT\n")
        r = subprocess.run([emu, "-d", "C=dir:" + d], input=script,
                           capture_output=True, text=True, errors="replace")
        out = re.sub(r"[^\t\n\x20-\x7e]", "", r.stdout)
        if keep:
            print("kept", d)
        return out, n
    finally:
        if not keep:
            shutil.rmtree(d)


def classify(op, line):
    """Sort a mismatch into: nan, tiny (exact-underflow flag or subnormal
    double rounding, both expected from the draft-standard design), range
    (float-to-integer out of range or NaN, where the FPE reports integer
    overflow instead of invalid), flags (other flag differences) or value
    (a real numeric difference)."""
    t = line.split()
    ops = [(int(t[1], 16), int(t[2], 16)), (int(t[3], 16), int(t[4], 16))]
    got = (int(t[6], 16), int(t[7], 16)), int(t[8], 16)
    exp = (int(t[10], 16), int(t[11], 16)), int(t[12], 16)
    fn, arity, kind, _ = OPDEF[op]
    def isnan(h, l, dbl):
        if dbl:
            return (h & 0x7FF00000) == 0x7FF00000 and bool(h & 0xFFFFF or l)
        return (h & 0x7F800000) == 0x7F800000 and bool(h & 0x7FFFFF)
    srcdbl = fn.startswith("f64") and kind != "d" or op[0] == "d" and kind == "b" \
        or (kind == "d" and fn.startswith("f64"))
    srcnan = any(isnan(h, l, srcdbl) for h, l in ops[:arity]) \
        if fn.startswith(("f32", "f64")) else False
    if kind == "b":
        if got[0][0] != exp[0][0]:
            return "nan" if srcnan else "value"
        return "nan" if srcnan else "flags"
    if kind == "i":
        if got[0][0] == exp[0][0] and got[1] == exp[1]:
            return "ok"
        if exp[1] & 0x01 or got[1] & 0x80:
            return "range"
        return "value" if got[0][0] != exp[0][0] else "flags"
    dbl = kind == "d"
    if isnan(*got[0], dbl) != isnan(*exp[0], dbl):
        return "value"
    if isnan(*got[0], dbl):
        return "nan"
    n = 2 if dbl else 1
    if exp[1] & 0x04:
        return "tiny"
    if got[0][:n] == exp[0][:n]:
        if got[1] ^ exp[1] == 0x04 and not exp[1] & 0x10:
            return "tiny"
        return "flags"
    return "value"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ops", nargs="+")
    ap.add_argument("-t", default="z8001")
    ap.add_argument("-r", default="near_even", choices=MODES)
    ap.add_argument("-n", type=int, default=0)
    ap.add_argument("-k", action="store_true")
    ap.add_argument("-b", action="store_true",
                    help="expect underflow tininess detected before rounding")
    ap.add_argument("-a", action="store_true",
                    help="affine infinity mode (default is the FPE reset state)")
    a = ap.parse_args()
    gen = os.environ.get("TESTFLOAT_GEN") or shutil.which("testfloat_gen")
    if not gen:
        sys.exit("testfloat_gen not found (set TESTFLOAT_GEN)")
    ops = NAMES if a.ops == ["all"] else a.ops
    rc = 0
    for op in ops:
        out, n = run(op, a.r, a.n, a.t, gen, a.k, a.a, a.b)
        bad = [l for l in out.split("\n") if l.startswith("BAD ")]
        m = re.search(r"FPVEC DONE (\d+) (\d+)", out)
        if not m:
            print("%s %s: run did not finish\n%s" % (op, a.r, out[-800:]))
            rc = 1
            continue
        kinds = {}
        for l in bad:
            kinds.setdefault(classify(op, l), []).append(l)
        print("%s %s: %s vectors, %s mismatches (value %d, flags %d, tiny %d, range %d, nan %d)"
              % (op, a.r, m.group(1), m.group(2), len(kinds.get("value", [])),
                 len(kinds.get("flags", [])), len(kinds.get("tiny", [])),
                 len(kinds.get("range", [])), len(kinds.get("nan", []))))
        for k in ("value", "flags", "range", "nan"):
            for l in kinds.get(k, [])[:5]:
                print("   %-5s %s" % (k, l))
        if kinds.get("value") or kinds.get("flags"):
            rc = 1
    sys.exit(rc)


main()
