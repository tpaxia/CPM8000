# CP/M-8000 1.1 Distribution Disk Images

Two image sets of the same CP/M-8000 1.1 release:

- `distribution/CPM_8000_1.1/` — `REL11A/B/C.IMG`, `GAMES.IMG`, `MISC11.IMG`,
  `TEXT11.IMG` (6 disks, Olivetti M20 format). This is the set the project
  uses: the emulator mounts it and `scripts/regenerate-cpm8k.sh` extracts
  `src/cpm8k/` from it.
- `distribution/CPM_8000_1.1_8inch/` — `8K_1OF4.IMG` … `8K_4OF4.IMG` (4 disks,
  8" IBM-3740 format). Archival; not used by the build.

## Format

| | 8" set (`8K_*OF4`) | M20 set (`REL11A` etc.) |
|---|---|---|
| Bytes/image | 256,256 | 286,720 |
| Geometry | 77 trk × 26 sec × 128 B | 70 trk × 16 sec × 256 B |
| Format | IBM-3740 8" SSSD | Olivetti M20 DS/DD |
| cpmtools `-f` | `ibm-3740` (built in) | `m20` (see `src/diskdefs_m20.mame`) |
| Disks | 4 | 6 |
| Total files | 60 | 76 |
| Read by this emulator? | No (wrong geometry) | Yes (`-d A=img:…`) |

## Disk contents

### 8" set — organized by part number (binaries + sources mixed)

| Disk | Files | Rough theme |
|---|---|---|
| `8K_1OF4` | copy, cpm.sys, cpmldr.sys, ddt, dump, ed, format, nmz8k, opt.c/.o, opt1.c/.o, pip, putboot.z8k | system + core utilities |
| `8K_2OF4` | ar8k, ld8k, sizez8k, stat, zcc, zcc1, zcc2 | tools + compiler (part) |
| `8K_3OF4` | asz8k.pd, asz8k, ctype.h, errno.h, libcpm.a, option.h, setjmp.h, signal.h, startup.8kn/.o, stdio.h, xcon, xdump, xout.h, zcc3 | assembler, lib, headers |
| `8K_4OF4` | bios.c, bios.rel, bios.sub, bios*.8kn, cpmldr.rel, cpmsys.rel/2.rel, cpmsys.sub, fpe.o, fpedep.o, lbiosasm.8kn, ldrbdos.rel, makeldr.sub, mkputbt.sub, putboot.c, readme, syscall.8kn, trk.o | BIOS/system sources + relocatables |

### M20 set — organized by function

| Disk | Purpose | Files |
|---|---|---|
| `REL11A` | **Bootable runtime** (mount this to boot) | cpm.sys + COPY, DDT, DUMP, ED, FORMAT, PIP, SIZEZ8K, STAT, readme |
| `REL11B` | **Toolchain / relocatable building** | AR8K, ASZ8K (+asz8k.pd), LD8K, LD8K2, NMZ8K, PUTBOOT, LIBCPM.A |
| `REL11C` | **C compiler + misc tools** | ZCC + ZCC1/2/3, XCON, XDUMP, PAUSE |
| `GAMES` | **Sample C programs** | WUMP, TICTAC (sources `.c/.s/.o/.z8k/.sub`) + headers cpm.h, portab.h, sgtty.h, stdio.h |
| `MISC11` | **Relocatables & objects** (rebuild the system) | bios.rel, cpmldr.rel, cpmldr.sys, cpmsys.rel, cpmsys2.rel, fpe.o, fpedep.o, ldrbdos.rel, opt.o, opt1.o, startup.o, trk.o |
| `TEXT11` | **Human-readable sources** | bios.c, bios*.8kn, *.sub build scripts, C headers, syscall.8kn, startup.8kn, putboot.c, opt.c/opt1.c, trk.8kn |

## How the two sets compare

All 60 files of the 8" set are also in the M20 set. Comparing bytes
(accounting for CP/M's 128-byte record padding):

- **58 / 60 files have the same content** — identical except within the final
  partial record. The binaries (`.z8k`, `.rel`, `.o`, `.sys`, `.a`) are the
  same builds; for text files the difference is trailing EOF/whitespace
  padding, or leftover buffer junk in the 8" copies' last record (e.g.
  `opt.c`).
- **Two files differ:**

| File | Difference in the M20 copy |
|---|---|
| `bios.sub` | No `-w` flag on the `ld8k` line; no `pip c:bios.rel[g5]=bios.rel` line or stray `rel …` line. |
| `bios.c` | Cosmetic only: `/*  End of C Bios */` (extra space) plus two trailing blank lines. |

- **Only in the M20 set (16 files):** the whole `GAMES` disk (`cpm.h`,
  `portab.h`, `sgtty.h`, a second `stdio.h`, `tictac.{c,s,o,z8k,sub}`,
  `wump.{c,o,z8k,sub}`), `ld8k2.z8k` (a second linker binary), `pause.z8k`,
  and `trk.8kn` (the source for `trk.o`).

### `asz8k.pd` (assembler predef)

The two image copies differ only after the M20 copy's end-of-file marker
(the 8" copy is one record longer). The checked-in `src/cpm8k/asz8k.pd` and
`src/asm8k/asz8k.pd` are byte-identical to the M20 (`REL11B`) copy.

## Reading the images

```sh
# cpmtools uses ./diskdefs instead of the system file when one exists, so it
# must hold both ibm-3740 (system file) and m20 (src/diskdefs_m20.mame):
mkdir -p /tmp/cpmimg && cd /tmp/cpmimg
cat /opt/homebrew/share/diskdefs $CPM8000/src/diskdefs_m20.mame > diskdefs

# list / extract (-T raw: libdsk's autodetect fails on these images):
cpmls -T raw -f ibm-3740 -l $CPM8000/distribution/CPM_8000_1.1_8inch/8K_1OF4.IMG
cpmls -T raw -f m20      -l $CPM8000/distribution/CPM_8000_1.1/REL11A.IMG
cpmcp -T raw -f m20 $CPM8000/distribution/CPM_8000_1.1/REL11A.IMG 0:cpm.sys ./cpm.sys
```

When comparing extracted files, ignore differences confined to the final
128-byte record — that is CP/M padding, not a content change.
