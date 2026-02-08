# CP/M-8000 Emulator - Progress & Issues

## Status: Fully functional. CCP, built-in commands, file I/O, and transient programs all work.

## What Works
- Z8001 CPU emulator with native trap/IRET mechanism for all SC calls
- CCP boots from cpm.sys (COFF binary loaded into segment 0x0B)
- CP/M-8000 banner displays: "CP/M-8000(tm) Version 1.1 12/19/84"
- `A>` prompt appears
- BDOS functions dispatched correctly (SC #2 via assembly trap handler)
- BIOS functions dispatched correctly (SC #3 via assembly trap handler)
- Memory management SC #0 and SC #1 (xfer, mem_cpy, map_adr) implemented
- Context transfer (xfer) works via native IRET segment switching
- Transient program loading (PGMLD, x.out format) and execution
- Console I/O works (raw mode for tty, pipe support with EOF detection)
- File system mapped to host directories (drives/A, drives/B, etc.)
- DIR command lists files from host directories
- TYPE command displays file contents
- STAT runs correctly (transient program via xfer)
- EXIT command quits the emulator; Ctrl-D also works
- Warm boot cycle works (program exit -> CCP restart)
- Pipe input works: `echo "DIR" | build/emu/cpm8k` correctly lists files
- Build pipeline: `make emu`

## Bugs Fixed

### 1. Pipe input consumed during banner output (ROOT CAUSE of hang)
The BDOS C_WRITE function checks for Ctrl-S/Ctrl-C by calling BIOS
console_status() + CONIN before each character output. When stdin is a
pipe, console_status() returned true (pipe has data), so the BDOS read
and discarded pipe characters during banner output. By the time
C_READSTR was called, the pipe was at EOF.

**Fix**: `console_status()` now returns false when stdin is not a tty,
preventing the BDOS from consuming piped input during output.

### 2. Memory Region Table format was wrong
The MRT format didn't match the manual (Section 4, Function 18). The
manual specifies: 16-bit entry count + 4 regions of {32-bit base,
32-bit length}. The old code used 8-byte entries with a zero terminator.

**Fix**: Rewrote `build_mrt()` to produce the correct format with
4 regions: merged I/D, split I/D code, split I/D data, data access
to instruction space.

### 3. BIOS CONOUT read wrong register
BIOS Function 4 (CONOUT) read the character from R7 (second parameter)
instead of R5 (first parameter) per the manual. In practice both
registers had the same value because the BDOS sets both, but the fix
is correct per the spec.

### 4. C_READSTR didn't null-terminate the buffer
The CP/M-8000 CCP is written in C and may expect null-terminated
strings. Added null byte after the last input character.

### 5. Added EXIT command and Ctrl-D to quit emulator
- Typing "EXIT" at the A> prompt cleanly exits the emulator
- Pressing Ctrl-D exits immediately
- Pipe EOF at start of input also exits cleanly

### 6. xfer (context switch) failed to change CPU segment
The original design intercepted all SC instructions via a C++ callback
*before* the Z8001's native PSA dispatch. This bypassed the hardware
trap/IRET mechanism. When the CCP called xfer to launch a transient
program in a different segment (e.g. 0x0A), `set_pc()` in non-segmented
mode couldn't change the CPU segment because the callback ran in the
caller's CPU mode.

**Fix**: Moved all SC handling from the C++ callback to BIOS assembly
trap handlers. SC instructions now trigger the Z8001 native hardware
trap, which saves PC/FCW to the system stack and dispatches through the
PSA to the BIOS assembly `_trap` handler. For xfer, the context block
is copied onto the trap stack frame, and IRET naturally pops the full
32-bit segmented PC + new FCW, changing segments correctly. For
BDOS/BIOS calls, the assembly handlers bridge to C++ via synchronous
I/O port OUT instructions.

## Architecture

### SC Handling: Native Trap Mechanism

All system calls (SC #0 through SC #3) flow through the Z8001 hardware
trap mechanism rather than being intercepted in C++:

```
SC instruction
  -> Z8001 hardware trap (pushes PC, FCW, SC instruction to system stack)
  -> PSA dispatch (loads new FCW + PC from Process Status Area)
  -> sc_trap entry point in BIOS assembly
  -> _trap (saves r0-r13, switches to NONSEG, extracts SC number)
  -> trap_disp (looks up handler in _trapvec table)
  -> handler (bdossc / biossc / memsc / xfersc)
  -> _trap_ret (restores registers)
  -> IRET (pops FCW + full segmented PC from stack)
```

This design is critical for xfer (context transfer), where the new
program's registers, FCW, and segmented PC are copied onto the trap
stack frame. When IRET executes, it naturally pops the new context,
including switching to a different memory segment.

### I/O Port Bridging

BDOS and BIOS handlers in assembly bridge to C++ via privileged OUT
instructions to dedicated I/O ports. The OUT executes synchronously
during the trap handler (which runs in system mode), so the C++ I/O
handler can read/write CPU registers directly:

| Port | Purpose | Registers Read | Registers Written |
|------|---------|----------------|-------------------|
| 0xF0 | BDOS call | r4=caller_seg, r5=func, rr6=param | rr6=result |
| 0xF2 | BIOS call | r2=caller_seg, r3=func, rr4=P1, rr6=P2 | rr6=result |
| 0xF4 | map_adr | r4=caller_seg, r5=space, rr6=addr | r6=mapped_seg |
| 0xF6 | mem_cpy | rr2=length, rr4=dest, rr6=source | rr6=result |

The caller's PC segment is read from the trap stack frame (`scseg`
field) by the assembly handler and placed in a register before the OUT.
The `scseg` word uses Z8001 segmented address format: segment number
in the high byte with bit 7 set, so extraction is `(reg >> 8) & 0x7F`.

After the OUT returns, the assembly handler writes the C++ return values
(r6/r7) back into the trap stack frame so they're restored by IRET.

### Trap Stack Frame Layout

The trap frame is built by the hardware (PC, FCW, SC instruction) and
the `_trap` handler (registers, stack pointers):

```
Offset  Field     Description
0x00    cr0       Saved r0
0x02    cr1       Saved r1
  ...
0x1A    cr13      Saved r13
0x1C    nr14      Saved r14 (seg stack ptr)
0x1E    nr15      Saved r15 (nonseg stack ptr)
0x20    scinst    SC instruction word (pushed by hardware)
0x22    scfcw     Saved FCW (pushed by hardware)
0x24    scseg     Saved PC segment word (pushed by hardware)
0x26    scpc      Saved PC offset (pushed by hardware)
0x28    FRAMESZ   Total frame size (40 bytes = 20 words)
```

For xfer, the entire frame is overwritten by `ldir` from the context
block, replacing all registers, FCW, and PC with the new program's
values.

### SC Handler Dispatch

| SC # | Handler | Purpose |
|------|---------|---------|
| SC #0 | memsc | Memory operations (same as SC #1) |
| SC #1 | memsc | xfer / set_user_seg / map_adr / mem_cpy |
| SC #2 | bdossc | BDOS function call |
| SC #3 | biossc | BIOS function call |

The `memsc` handler dispatches on the caller's r5 register:
- `0xFFFE` -> xfer (context transfer via frame copy + IRET)
- `0xFFFF` -> set_user_seg (no-op in emulator)
- `rr2 != 0` -> mem_cpy (copy via I/O port)
- else -> map_adr (address mapping via I/O port)

### Files

#### z8000_emu/ (Z8001 CPU Emulator)
- `include/z8000.h` - CPU device class, register access, trace support
- `include/z8000_intf.h` - Abstract memory/IO bus interfaces
- `src/z8000.cpp` - CPU emulation core, Interrupt() handler, run loop

#### bios/emu/ (Thin BIOS for Emulator)
- `biosdef.s` - Shared definitions (SC numbers, frame offsets, I/O ports)
- `biosboot.s` - Bootstrap: clear BSS, set stack/PSAP, init traps, jump to CCP
- `biostrap.s` - Trap handlers: _trap, bdossc, biossc, memsc, xfersc, trapinit
- `cpmsys.x` - Linker script
- `Makefile` - Links biosboot.o + biostrap.o + cpmsys.o -> cpm.sys

#### src/cpm8kemu/ (Host Emulator)
- `main.cpp` - Entry point, COFF loader, EmuIO class (I/O port handlers)
- `cpm8k_bdos.h/.cpp` - BDOS SC #2 handler (functions 0-59)
- `cpm8k_bios.h/.cpp` - BIOS SC #3 handler
- `cpm8k_console.h/.cpp` - Terminal raw mode I/O
- `cpm8k_file.h/.cpp` - CP/M file ops -> host filesystem
- `cpm8k_mem.h` - SegmentedMemory class (512KB, segment translation)
- `Makefile` - Builds cpm8k binary

### Memory Map
| Segment | Physical | Purpose |
|---------|----------|---------|
| 0x02 | 0x00000 | PSA (at offset 0x100) |
| 0x08 | 0x10000 | TPA split I/D |
| 0x0A | 0x20000 | TPA merged I/D |
| 0x0B | 0x30000 | System: CCP + BIOS data |

### Key Design Decisions
- SC calls use Z8001 native trap/IRET mechanism for correct segment switching
- Assembly trap handlers bridge to C++ via synchronous I/O port OUT
- Caller's segment extracted from trap frame `scseg` field by assembly
- xfer overwrites the trap frame and relies on IRET for context switch
- CCP (seg 0x0B) and user programs (seg 0x0A) both use BDOS
- DMA address includes caller's segment for correct memory access
- FCB access uses caller's segment via `m_caller_seg`
- console_status() returns false for non-tty stdin to prevent BDOS
  from consuming pipe data during output (Ctrl-S/C check)

## Build & Test Commands
```bash
make emu          # Build everything (lib + bios-emu + emulator)

# Interactive test:
build/emu/cpm8k

# With CPU trace:
build/emu/cpm8k -t

# Piped test:
echo "DIR" | build/emu/cpm8k 2>emu_stderr.txt

# Multiple commands via pipe:
printf "DIR\nEXIT\n" | build/emu/cpm8k 2>/dev/null
```

## Known Issues / TODO
- BDOS function 46 (F_GETAZERO) unimplemented (STAT calls it, falls through gracefully)
- No Ctrl-C signal handling in interactive mode
- BIOS disk functions are stubs (all file I/O goes through host BDOS)
