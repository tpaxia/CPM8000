# Hosted Z8001/Z8002 Emulator Architecture

The hosted emulator builds two executables from the same C++ implementation:
`cpm8k-z8001` for the segmented Z8001 and `cpm8k-z8002` for the
non-segmented Z8002.  The filesystem, BDOS routing, disk-image I/O, console,
program-layout logic, and physical RAM implementation are shared.  CPU trap
frames, address translation, memory-region descriptions, and the thin BIOS
assembly are target-specific.

## Target selection

CMake defines `CPM8K_SEGMENTED` or `CPM8K_NONSEGMENTED` for the corresponding
executable.  `cpm8k_target.h` then selects `SegmentedTarget` or
`NonSegmentedTarget`.  Each target supplies its CPU type, memory translator,
program-loader policy, boot state, SC address-mapping policy, and matching
host-loaded `cpm.sys`.

The target-specific BIOS sources are in:

- `bios-z8001/` for the segmented Z8001
- `bios-z8002/` for the non-segmented Z8002

## System-call frames

An `SC` instruction enters the CPU's native trap mechanism.  The hardware
frame is extended by the BIOS trap handler with the caller's registers, then
dispatched to the memory, BDOS, or BIOS handler.  Returning through `IRET`
consumes the CPU-specific hardware portion of the frame.

The Z8001 frame is 40 bytes:

| Field | Size |
| --- | ---: |
| R0-R13 | 28 bytes |
| Normal-mode R14 and R15 | 4 bytes |
| SC instruction | 2 bytes |
| FCW | 2 bytes |
| PC segment | 2 bytes |
| PC offset | 2 bytes |

The Z8002 has no PC-segment word in its hardware trap frame, so its internal
frame is 38 bytes.  Its saved return address contains only the FCW and 16-bit
PC.  The handler call itself also differs: the Z8001 has a four-byte segmented
return address, while the Z8002 has a two-byte return address.  This accounts
for the `+4` and `+2` frame offsets in the two `biostrap.s` implementations.

On the Z8001, handlers obtain the caller's segment directly from `scseg`.  A
Z8002 has no such field, so its saved FCW distinguishes system from normal
mode and the current code or data bank supplies the normal-mode address tag.

## Transfer-control context

CP/M-8000 defines the context passed to `xfer` as a 40-byte public structure,
including a PC-segment word even on a Z8002.  This preserves the CP/M ABI
across the two processors.

The Z8001 `xfersc` can copy the complete context directly over its 40-byte
trap frame.  The Z8002 `xfersc` accepts the same public context but omits the
compatibility PC-segment word while constructing its 38-byte frame.  In both
cases `_trap_ret` restores the registers and `IRET` transfers control to the
new context.

## Physical memory and address translation

Both hosted targets use the same 512 KiB physical RAM array and bus.  Their
logical-to-physical translation differs:

- `SegmentedMemory` maintains Z8001 program- and data-space maps indexed by
  the seven-bit segment number, with separate system-mode mappings where
  needed.
- `Z8002Memory` maps each 16-bit logical address through the currently
  selected user code or data bank.  System-mode accesses use bank zero.

The shared `mem_cpy` implementation operates on the physical address tags
returned by the target's `map_adr` policy.  The tags are an internal BIOS
representation: Z8001 targets use segment numbers, while the Z8002 target
uses bank tags.

## Memory Region Tables

Each target builds the Memory Region Table returned by the BIOS to CP/M:

- The Z8001 table advertises segment `0x0a` for merged instruction/data and
  segment `0x08` for split instruction/data operation.
- The Z8002 table advertises bank tag `0x0100` for merged memory and program
  space, and `0x0200` for split data space.

These representations describe the same basic 64 KiB TPA choices through
the target's own physical-address vocabulary.

## Program loading and TPA allocation

The x.out parser and allocation algorithm are shared.  Target-specific loader
policies choose the code, data, and execution tags:

- The Z8001 accepts merged and split non-segmented programs as well as
  segmented x.out programs.
- The Z8002 accepts merged and split non-segmented programs and rejects
  segmented executables.

Code, constants, data, and BSS are loaded upward from the start of their
selected TPA regions.  For a split program, code and data use different
physical regions.  The basepage and stack are placed near the top of the
64 KiB data region, with the stack growing downward.  The basepage's free
length describes the remaining space between the loaded data/BSS and the
reserved basepage/stack area.

The guest C runtime's `_brk` implementation is common to both targets.  It
tracks a 16-bit break address in the current data space and rejects requests
that would collide with the descending stack.  There is no separate hosted
Z8001 or Z8002 heap allocator.

## Source map

| Responsibility | Source |
| --- | --- |
| Target selection and policies | `cpm8k_target.h` |
| Memory translators and shared physical RAM | `cpm8k_mem.h` |
| MRT construction and machine startup | `main.cpp` |
| Shared program loading and allocation | `cpm8k_bdos.cpp` |
| Z8001 trap frame and SC handlers | `bios-z8001/biostrap.s`, `bios-z8001/biosdef.s` |
| Z8002 trap frame and SC handlers | `bios-z8002/biostrap.s`, `bios-z8002/biosdef.s` |
| Shared guest heap check | `../cpm8k/startup.8kn` |
