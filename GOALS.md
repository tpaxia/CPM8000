# CPM8000 — Project Goals

This document records the purpose and direction of the CPM8000 project: a
port of CP/M-8000 to the Olivetti M20 (Z8001) together with a hosted
emulator and a customizable build system.

## Goals

### 1. Emulate CP/M-8000 without emulating the exact hardware

Provide a *functional* emulator of CP/M-8000, not a cycle/hardware-accurate
machine emulator like MAME. The emulator runs the real Z8001 CPU core and
the real CP/M-8000 system, but the surrounding hardware (disk controller,
serial, etc.) is modeled at the functional level — services are provided by
the host rather than by emulating specific chips. The point is to *run and
develop CP/M-8000 software*, not to reproduce a particular board down to the
register.

### 2. Provide a build system for the BIOS and commands so the system can be customized

A reproducible build pipeline that takes the CP/M-8000 sources, the BIOS, and
the system utilities and produces a working system, so that the BIOS and the
commands can be modified and rebuilt. Customizing the OS (BIOS behavior,
built-ins, utilities) should be a normal, supported workflow — not a one-off
patch.

### 3. Write new, or extend existing, system utilities to add new features

Go beyond restoring the original system: add capabilities CP/M-8000 never
had. Examples:

- An **`fdisk`-style utility** for partitioning the hard drive.
- **BIOS support for multiple operating systems on the same hard drive**
  (boot/select between OSes).

### 4. Make it easy to support new CP/M-8000 boards

The BIOS and build system should be structured so that bringing up a new
Z8001/CP/M-8000 board (different I/O addresses, serial, disk interface,
memory map) is a well-defined, low-friction task rather than a rewrite.

### 5. Port and verify other tools

Bring additional development tools onto the platform and verify they work —
for example **C compilers** and other toolchains — so CP/M-8000 becomes a
viable environment for real software development.

## Non-goals

- Cycle-accurate or chip-accurate hardware emulation (that is MAME's domain).
