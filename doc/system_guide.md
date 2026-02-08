# CP/M-8000 System Guide

### CP/M-8000

### Operating System

### System Guide

Copyright   1983

Digital Research
P.O. Box 579
167 Central Avenue
Pacific Grove, CA 93950
(408) 649-3896
TWX 910 360 5001

All Rights Reserved

COPYRIGHT

Copyright   1983 by Digital Research.  All rights reserved.
No part of this publication may be reproduced, transmitted,
transcribed, stored in a retrieval system, or translated
into any language or computer language, in any form or
by any means, electronic, mechanical, magnetic, optical,
chemical, manual or otherwise, without the prior written
permission of Digital Research, Post Office Box 579, Pacific
Grove, California, 93950.

DISCLAIMER

Digital Research makes no representations or warranties with
respect to the contents hereof and specifically disclaims
any implied warranties of merchantability or fitness for
any particular purpose.  Further, Digital Research reserves the
right to revise this publication and to make changes from
time to time in the content hereof without obligation of
Digital Research to notify any person of such revision or
changes.

TRADEMARKS

CP/M and CP/M-86 are registered trademarks of Digital Research.  CP/M-80,
CP/M-8000, DDT, and MP/M are trademarks of Digital Research.  Z80 and
Z8000 are trademarks of Zilog, Inc.  VAX/VMS is
a trademark of Digital Equipment Corporation.  UNIX is a trademark of
Bell Laboratories.

The *CP/M-8000 Operating System System Guide* was prepared using the Digital Research TEX Text Formatter and printed
in the United States of America.

********************************
*  First Edition:  March 1983  *
********************************

## FOREWORD

CP/M-8000   is a single-user general purpose operating system.
It is designed for use with any disk-based computer using a Zilog Z8000 or
compatible processor.  CP/M-8000 is modular in design, and can be modified to
suit the needs of a particular installation.

The hardware interface for a particular hardware environment is supported by
the OEM or CP/M-8000 distributer.  Digital Research supports the user
interface to CP/M-8000 as documented in the *CP/M-8000 Operating System User's Guide.*   Digital Research does not support any additions or modifications made to
CP/M-8000 by the OEM or distributer.

### Purpose and Audience

This manual is intended to provide the information needed by a
systems programmer in adapting CP/M-8000 to a particular hardware
environment.   A substantial degree of programming expertise is
assumed on the part of the reader, and it is not expected that
typical users of CP/M-8000 will need or want to read this manual.

### Prerequisites and Related Publications

In addition to this manual, the reader should be familiar with the
architecture of the Zilog Z8000 as described in the *Zilog 16-Bit Microprocessor User's Manual* (third edition), the *CP/M-8000 User's and Programmer's Guides,* and, of course, the details of the hardware environment where CP/M-8000 is
to be implemented.

### How This Book is Organized

Section 1 presents an overview of CP/M-8000 and describes its major
components.  Section 2 discusses the adaptation of CP/M-8000 for your specific
hardware system.  Section 3 discusses bootstrap procedures and related
information.  Section 4 describes each BIOS function including entry
parameters and return values.  Section 5 describes the process of creating a
BIOS for a custom hardware interface.  Section 6 discusses approaches to
debugging your BIOS.  Section 7 provides information on using the distributed
version of CP/M-8000 if you have a ===???===   system.

Appendix A describes the contents of the CP/M-8000 distribution disks.
Appendixes B, C, and D are listings of various BIOSes.  Appendix E
contains a listing of the PUTBOOT utility program.

## Table of Contents

### 1  System Overview

### 2  System Generation

### 3  Bootstrap Procedures

### 4  BIOS Functions

## Table of Contents

### 5  Creating a BIOS

### 6  Installing and Adapting the Distributed BIOS and CP/M-8000

### 7  Cold Boot Automatic Command Execution

   7.2  Setting up Cold Boot Automatic Command Execution  . .  51

### 8  The PUTBOOT Utility

## Appendixes

### A

### B

### C

### D

### E

### F

### Tables and Figures

### Tables

### Figures

## Section 1

### System Overview

### 1.1  Introduction

CP/M-8000 is a single-user, general purpose operating system for
microcomputers based on the Zilog Z8000 or equivalent microprocessor
chip.  It is designed to be adaptable to almost any hardware environment, and
can be readily customized for particular hardware systems.

CP/M-8000 is equivalent to other CP/M   systems with changes
dictated by the Z8000 architecture.  In particular, CP/M-8000 supports the
very large segmented address space of the Z8000 family.
The CP/M-8000 file system
is upwardly compatible with CP/M-80   version 2.2 and CP/M-86   Version
1.1.  The CP/M-8000 file structure allows files of up to 32 megabytes per
file.   CP/M-8000 supports from one to sixteen disk drives with as many as
512 megabytes per drive.

The entire CP/M-8000 operating system resides in memory at all times,
and is not reloaded at a warm start.  CP/M-8000 can be configured to reside in
any portion of memory.
The remainder of the address space is available for applications programs,
and is called the transient program area, TPA.
The TPA is assumed to consist of one or more complete (64 Kbyte)
memory segments.
CP/M-8000 supports both segmented and non-segmented user programs, and
supports the splitting of user program and data into separate addressing
spaces.

Several terms used throughout this manual are defined in Table 1-1.

##### Table 1-1.   CP/M-8000 Terms

| Term             | Meaning                                                                                                  |
| ---------------- | -------------------------------------------------------------------------------------------------------- |
| nibble           | 4-bit half-byte                                                                                          |
| byte             | 8-bit value                                                                                              |
| word             | 16-bit value                                                                                             |
| longword         | 32-bit value                                                                                             |
| address          | 32-bit identifier of a storage location                                                                  |
| physical address | address of a location in real memory                                                                     |
| logical address  | address as issued by a program, possibly requiring translation into a physical address.                  |
| offset           | a value defining an address in storage; a fixed displacement from some other address                     |
| text segment     | program section containing machine instructions                                                          |
| data segment     | program section containing initialized data                                                              |
| block storage    | program section containing uninitialized data segment (bss)                                              |
| absolute         | describes a program which must reside at a fixed memory address.                                         |
| relocatable      | describes a program which includes relocation information so it can be loaded into memory at any address |

The CP/M-8000 programming model is described in detail in the *CP/M-8000 Operating System Programmer's Guide.*   To summarize that model briefly, CP/M-8000 supports four segments within
a program:  text, data, block storage segment (bss), and stack.  When a
program is loaded, CP/M-8000 allocates space for all four segments in the TPA,
and loads the text and data segments.  A transient program may manage free
memory using values stored by CP/M-8000 in its base page.

If the program is to run in segmented mode,
the allocation of program segments to logical address segments must
have been done at link time.  If the program is to run in non-segmented
mode, however, information in the Memory Region Table is used to
decide which physical segments to run the program in.  If the program
is to run with split program and data spaces, two physical segments
are required (with the data, bss, and stack in the same physical
segment), otherwise only a single physical segment is used.

```
                             USER


                       User Interface

                            (CCP)


                        Programming
                        Interface


                         (BDOS)


                         Hardware
                        Interface

                         (BIOS)


                   HARDWARE ENVIRONMENT
```

##### Figure 1-1.  CP/M-8000 Interfaces

### 1.2  CP/M-8000 Organization

CP/M-8000 comprises three system modules:  the Console Command Processor (CCP)
the Basic Disk Operating System (BDOS) and the Basic Input/Output System
(BIOS).  These modules are linked together to form the operating system.
They are discussed individually in this section.

### 1.3  Memory Layout

The CP/M-8000 operating system can reside anywhere in memory.
The location of CP/M-8000 is defined
during system generation.  Typically the system occupies a
segment which is logically separated from the TPA.

The TPA for non-segmented programs consists of one or two 64 Kbyte
segments, one for program and one for data.  (Some programs
expect program and data to be mixed in one segment.  The segment
in which such programs are run may be the same as or different
from the segments for separated program and data.)  The TPA for
segmented programs consists of up to 128 segments.
The mapping from logical addresses (which consist of a 7-bit segment
number and a 16-bit offset) into physical addresses is done by
system-specific hardware, and the BIOS contains memory management
operations to map addresses and copy blocks of memory.

##### Figure 1-2.  Typical CP/M-8000 Memory Layout

### 1.4  Console Command Processor (CCP)

The Console Command Processor, (CCP) provides the user interface to
CP/M-8000.  It uses the BDOS to read user commands and load programs,
and provides several built-in user commands.  It also provides
parsing of command lines entered at the console.

### 1.5  Basic Disk Operating System (BDOS)

The Basic Disk Operating System (BDOS) provides operating system services to
applications programs and to the CCP.  These include character I/O,
disk file I/O (the BDOS disk I/O operations comprise the CP/M-8000 file
system), program loading, and others.

### 1.6  Basic I/O System (BIOS)

The Basic Input Output System (BIOS) is the interface between CP/M-8000 and its
hardware environment.  All physical input and output is done by the
BIOS.  It includes all physical device drivers, tables defining disk
characteristics, and other hardware specific functions and tables.  The
CCP and BDOS do not change for different hardware environments because
all hardware dependencies have been concentrated in the BIOS.  Each hardware
configuration needs its own BIOS.  Section 4 describes the BIOS functions in
detail.  Section 5 discusses how to write a custom BIOS.  Sample BIOSes are
presented in the appendixes.

### 1.7  I/O Devices

CP/M-8000 recognizes two basic types of I/O devices:
character devices and disk drives.  Character devices are serial devices that
handle one character at a time.  Disk devices handle data in units of 128
bytes, called sectors, and provide a large number of sectors which can be
accessed in random, nonsequential, order.  In fact, real systems might have
devices with characteristics different from these.  It is the BIOS's
responsibility to resolve differences between the logical device models and
the actual physical devices.

#### 1.7.1  Character Devices

Character devices are input output devices which accept or supply
streams of ASCII characters to the computer.  Typical character devices are
consoles, printers, and modems.  In CP/M-8000 operations on character devices
are done one character at a time.  A character input device sends ASCII
CTRL-Z (1AH) to indicate end-of-file.

#### 1.7.2  Character Devices

Disk devices are used for file storage.  They are organized into sectors and
tracks.  Each sector contains 128 bytes of data.  (If sector sizes other than
128 bytes are used on the actual disk, then the BIOS must do a
logical-to-physical mapping to simulate 128-byte sectors to the rest of the
system.)
All disk I/O in CP/M-8000 is done in one-sector units.  A track is a group of
sectors.  The number of sectors on a track is a constant depending on the
particular device.  (The characteristics of a disk device are specified in
the  Disk  Parameter  Block for  that  device.  See Section 5.)
To locate a particular sector, the disk, track number,
and sector number must all be specified.

### 1.8  System Generation and Cold Start Operation

Generating a CP/M-8000 system is done by linking together the CCP, BDOS, and
BIOS to create a file called CPM.SYS, which is the operating system.
Section 2 discusses how to create CPM.SYS.  CPM.SYS is brought into memory
by a bootstrap loader which will typically reside on the first two tracks
of a system disk.  (The term system disk as used here simply means a disk
with the file CPM.SYS and a bootstrap loader.)  Creation of a bootstrap loader
is discussed in Section 3.

End of Section 1

## Section 2

### System Generation

### 2.1  Overview

This section describes how to build a custom version of CP/M-8000 by combining
your BIOS with the CCP and BDOS supplied by Digital Research to obtain a
CP/M-8000 operating system suitable for your specific hardware system.
Section 5 describes how to create a BIOS.

In this section, we assume that you have access to an already configured and
executable CP/M-8000 system.  If you do not, you should first read
Section 6, which discusses how you can make your first CP/M-8000 system
work.  === ACTUALLY, IT MAY BE SIMPLY IMPOSSIBLE ===

A CP/M-8000 operating system is generated by using the linker, LD8K, to
link together the system modules (CCP, BDOS, and BIOS) and
bind the system to an absolute memory location.
The resulting file is the configured operating system.  It is named CPM.SYS.

### 2.2  Creating CPM.SYS

The CCP and BDOS for CP/M-8000 are distributed in a library file named
CPMLIB.  You must link your BIOS with CPMLIB using the following command:

A>

### LO68 -R -UCPM -O CPM.REL CPMLIB BIOS.O

where BIOS.O is the compiled or assembled BIOS.  This creates CPM.SYS, which
is an absolute version of your system.

### 2.3   Relocating Utilities

Since the utilities all run in non-segmented mode, they do not need to
be relocated:  they will run in whatever segments you have assigned
for the TPA.  Note that the compiler and linker require separate
code and data segments; all other utilities supplied with the system
run in a single segment.

End of Section 2

## Section 3

### Bootstrap Procedures

### 3.1  Bootstrapping Overview

Bootstrap loading is the process of bringing the CP/M-8000 operating system
into memory and passing control to it.  Bootstrap loading is necessarily
hardware dependent, and it is not possible to discuss all possible variations
in this manual.  However, the manual presents a model of bootstrapping that is
applicable to most systems.
=== 	There are three bootstrap models:
===	The one presented here,
===	The Olivetti version, with the whole system on the boot tracks,
===	And the TRS-80 version, with the loader running on a Z80.

The model of bootstrapping which we present assumes that the CP/M-8000
operating system is to be loaded into memory from a disk in which the first
few tracks (typically the first two) are reserved for the operating system
and bootstrap routines, while the remainder of the disk contains the file
structure, consisting of a directory and disk files.  (The topic of disk
organization and parameters is discussed in Section 5.)  In our model,
the CP/M-8000 operating system resides in a disk file named CPM.SYS (described
in Section 2), and the system tracks contain a bootstrap loader program
(CPMLDR.SYS) which knows how to read CPM.SYS into memory and transfer
control to it.

Most systems have a boot procedure similar to the following:

1) When you press reset, or execute a boot command from a monitor ROM,
the hardware loads one or more sectors beginning at track 0, sector 1, into
memory at a predetermined address, and then jumps to that address.

2) The code that came from track 0, sector 1, and is now executing, is
typically a small bootstrap routine that loads the rest of the sectors on the
system tracks (containing CPMLDR) into another predetermined address in
memory, and then jumps to that address.  Note that if your hardware is
smart enough, steps 1 and 2 can be combined into one step.

3) The code loaded in step 2, which is now executing, is the CP/M Cold Boot
Loader, CPMLDR, which is an abbreviated version of CP/M-8000 itself.  CPMLDR
now finds the file CPM.SYS, loads it, and jumps to it.  A copy of CPM.SYS is
now in memory, executing.  This completes the bootstrapping process.

In order to create a CP/M-8000 diskette that can be booted, you need to know
how to create CPM.SYS (see Section 2.2),  how to create the Cold Boot Loader,
CPMLDR, and how to put CPMLDR onto your system tracks.  You must also
understand your hardware enough to be able to design a method for bringing
CPMLDR into memory and executing it.

### 3.2  Creating the Cold Boot Loader

CPMLDR is a miniature version of CP/M-8000.  It contains stripped versions of
the BDOS and BIOS, with only those functions which are needed to open the
CPM.SYS file and read it into memory.  CPMLDR will exist in at least two forms;
one form is the information in the system tracks, the other is a file named
CPMLDR.SYS which is created by the linker.  The term CPMLDR is used to
refer to either of these forms, but CPMLDR.SYS only refers to the file.

CPMLDR.SYS is generated using a procedure similar to that used in generating
CPM.SYS.  That is, a loader BIOS is linked with a loader system library, named
LDRLIB, to produce CPMLDR.SYS.  Additional modules may be linked in as required
by your hardware.  The resulting file is then loaded onto the system tracks
using a utility program named PUTBOOT.

#### 3.2.1  Writing a Loader BIOS

The loader BIOS is very similar to your ordinary BIOS; it just has fewer
functions, and the entry convention is slightly different.  The differences
are itemized below.

1) Only one disk needs to be supported.  The loader system selects only
drive A.  If you want to boot from a drive other than A, your loader BIOS
should be written to select that other drive when it receives a request to
select drive A.

2) The loader BIOS is not called through a trap; the loader BDOS calls an entry
point named _bios instead.  The parameters are still passed in registers, just
as in the normal BIOS.  Thus, your Function 0 does not need to
initialize a trap, the code that in a normal BIOS would be the Trap 3
handler should have the label _bios, and you exit from your loader BIOS with
an RTS instruction instead of an RTE.

3) Only the following BIOS functions need to be implemented:

0  (Init)    Called just once, should initialize hardware as necessary, no  return value necessary.  Note that Function 0 is called via _bios with the
function number equal to 0.  You do not need a separate _init entry point.

4  (Conout)  Used to print error messages during boot.  If you do not want
error messages, this function should just be an rts.

9  (Seldsk)  Called just once, to select drive A.

10 (Settrk)

11 (Setsec)

12 (Setdma)

13 (Read)

16 (Sectran)

18 (Get MRT) Not used now, but may be used in future releases.

22 (Set exception)

4) You do not need to include an allocation vector or a check vector, and the
Disk Parameter Header values that point to these can be anything.  However,
you still need a Disk Parameter Header, Disk Parameter Block, and directory
buffer.

It is possible to use the same source code for both your normal BIOS and
your loader BIOS if you use conditional compilation or assembly to
distinguish the two.
We have done this in our example BIOS for the EXORmacs.

#### 3.2.2 Building CPMLDR.SYS

Once you have written and compiled (or assembled) a loader BIOS, you can build
CPMLDR.SYS in a manner very similar to building CPM.SYS.  There is one
additional complication here:  the result of this step is
placed on the system tracks.  So, if you need a small prebooter to bring
in the bulk of CPMLDR, the prebooter must also be included in the link
you are about to do.  The details of what must be done are hardware dependent,
but the following example should help to clarify the concepts involved.

Suppose that your hardware reads track 0, sector 1, into memory at location
400H when reset is pressed, then jump to 400H.  Then your boot disk must
have a small program in that sector that can load the rest of the system
tracks into memory and execute the code that they contain.  Suppose that you
have written such a program, assembled it, and the assembler output is in
BOOT.O.  Also assume that your loader BIOS object code is in the file
LDRBIOS.O.  Then the following command links together the code that must
go on the system tracks.

A>

### lo68 -s -T400 -uldr -o cpmldr.sys boot.o ldrlib ldrbios.o

Once you have created CPMLDR.SYS in this way, you can use the PUTBOOT utility
to place it on the system tracks.  PUTBOOT is described in Section 8.
The command to place CPMLDR on the system tracks of drive A is:

A>

### putboot cpmldr.sys a

PUTBOOT reads the file CPMLDR.SYS, strips off the 28-byte command file
header, and puts the result on the specified drive.  You can now boot from
this disk, assuming that CPM.SYS is on the disk.

End of Section 3

## Section 4

### BIOS Functions

### 4.1  Introduction

All CP/M-8000 hardware dependencies are concentrated in subroutines that are
collectively referred to as the Basic I/O System (BIOS).  A CP/M-8000
system implementor can tailor CP/M-8000 to fit nearly any Z8000 operating
environment.  This section describes each BIOS function:  its calling
conventions, parameters, and the actions it must perform.
The discussion of Disk Definition Tables is treated separately in
Section 5.

When the BDOS calls a BIOS function, it places the function number in
register R3, and function parameters in registers RR4 and RR6.  It then
executes a SC #3 instruction.  R3 is always needed to specify the function,
but each function has its own requirements for other parameters, which are
described in the section describing the particular function.  The BIOS
returns results, if any, in register RR6.  The size of the result depends on
the particular function.

#### Note:

The system call handler in the BIOS must preserve at least registers
R8 through R15.  The handlers provided in most BIOSes
preserve all registers, except
for RR6 which is used to return results.
Of course, if the BIOS uses interrupts to service I/O, the
interrupt handlers will need to preserve registers.

Usually, user applications do not need to make direct use of BIOS functions.
However, when access to the BIOS is required by user software, it should
use the BDOS Direct BIOS Function, Call 50, instead of calling the BIOS with
a SC #3 instruction.  This rule ensures that applications remain compatible
with future systems.

The BIOS must also maintain a vector of "Exception Handler" addresses,
through which all system calls and traps are routed.  The vector
numbers have been selected to match the exception numbers
used in CPM-68K.  These numbers will be found in the Programmer's
Guide.

In addition to the general entry point via the SC #3 instruction,
the BIOS has two additional system call entry points.  SC #0 is
the entry point for the debugger's breakpoint instruction.  The
BIOS only has to vector this through the exception vector.  The
BIOS also has an entry point for a general-purpose memory-management
system call, SC #1, which is used to perform system-dependent
memory-management operations.  These operations are described
in section 4.2.

The Disk Parameter Header (DPH) and Disk Parameter Block (DPB) formats have
changed slightly from previous CP/M versions to accommodate the Z8000's
32-bit addresses.  The formats are described in Section 5.

##### Table 4-1.  BIOS Register Usage

```
                        Entry Parameters:

                     R3  = function code
                     RR4 = first parameter
                     RR6 = second parameter

                         Return Values:

                RL7 = byte values (8 bits)
                R7  = word values (16 bits)
                RR6 = longword values (32 bits)
```

The decimal BIOS function numbers and the functions
they correspond to are listed in Table 4-2.

##### Table 4-2.   BIOS Functions

| Number | Function                                                      |
| ------ | ------------------------------------------------------------- |
| 0      | Initialization (called for cold boot)                         |
| 1      | Warm Boot (called for warm start)                             |
| 2      | Console Status (check for console character ready)            |
| 3      | Read Console Character In                                     |
| 4      | Write Console Character Out                                   |
| 5      | List (write listing character out)                            |
| 6      | Auxiliary Output (write character to auxiliary output device) |
| 7      | Auxiliary Input (read from auxiliary input)                   |
| 8      | Home (move to track 00)                                       |
| 9      | Select Disk Drive                                             |
| 10     | Set Track Number                                              |
| 11     | Set Sector Number                                             |
| 12     | Set DMA Address                                               |
| 13     | Read Selected Sector                                          |
| 14     | Write Selected Sector                                         |
| 15     | Return List Status                                            |
| 16     | Sector Translate                                              |
| 18     | Get Memory Region Table Address                               |
| 19     | Get I/O Mapping Byte                                          |
| 20     | Set I/O Mapping Byte                                          |
| 21     | Flush Buffers                                                 |
| 22     | Set Exception Handler Address                                 |

                   FUNCTION 0:  INITIALIZATION
               Entry Parameters:
                  Register R3:  00H
               Returned   Value:
                  Register R3: User/Disk Numbers

This routine is entered on cold boot and must initialize the BIOS.
Function 0 is unique, in that it is not entered with a SC #3 instruction.
Instead, the BIOS has a global label, _init, which is the entry to this
routine.  On cold boot, Function 0 is called by a call _init.  When
initialization is done, exit is through a ret instruction.  Function 0
is responsible for initializing hardware if necessary, initializing
BIOS internal variables (such as IOBYTE) as needed,
setting up register RR6 as described below, setting the SC #3 vector to
point to the main BIOS entry point, and then exiting with a ret.

Function 0 returns a longword value.
The CCP uses this value to set the initial user number and the initial default
disk drive.  The least significant byte of RR6 is the disk number
(0 for drive A, 1 for drive B, and so on).  The next most significant byte is
the user number.  The high-order bytes should be zero.

The entry point to this function must be named _init and must be declared
global.  This function is called only once from the system at
system initialization.

```
                     FUNCTION 1:  WARM BOOT

                     Entry Parameters:
                        Register R3:  01H

                     Returned   Value:  None
```

This function is called whenever a program terminates.  Some reinitialization
of the hardware or software might occur.  When this function completes, it
jumps
directly to the entry point of the CCP, named _ccp.  Note that _ccp
must be declared as a global.

```
                   FUNCTION 2:  CONSOLE STATUS

              Entry Parameters:
                 Register R3: 02H

              Returned   Value:
                 Register R7: 00FFH if ready
                 Register R7: 0000H if not ready
```

This function returns the status of the currently assigned console device.
It returns 00FFH in register R7 when a character is ready to be read, or
0000H in register R7 when no console characters are ready.

```
               FUNCTION 3:  READ CONSOLE CHARACTER

                   Entry Parameters:
                      Register R3:  03H

                   Returned   Value:
                      Register R7: Character
```

This function reads the next console character into register R7.  If no
console character is ready, it waits until a character is typed before
returning.

```
              FUNCTION 4:  WRITE CONSOLE CHARACTER

                   Entry Parameters:
                      Register R3: 04H
                      Register R5: Character

                   Returned   Value: None
```

This function sends the character from register R5 to the console output
device.  The character is in ASCII.  You might want to include a delay or
filler
characters for a line-feed or carriage return, if your console device
requires some time interval at  the end  of the  line  (such as  a TI Silent
700
Terminal   ).  You can also filter out control characters which have
undesirable
effects on the console device.

```
               FUNCTION 5:  LIST CHARACTER OUTPUT

                   Entry Parameters:
                      Register R3: 05H
                      Register R5: Character

                   Returned   Value: None
```

This function sends an ASCII character from register R5 to the
currently assigned listing device.  If your list device requires some
communication protocol, it must be handled here.

```
                  FUNCTION 6:  AUXILIARY OUTPUT

                   Entry Parameters:
                      Register R3: 06H
                      Register R5: Character

                   Returned   Value: None
```

This function sends an ASCII character from register R5 to the
currently assigned auxiliary output device.

```
                  FUNCTION 7:  AUXILIARY INPUT

                   Entry Parameters:
                      Register R3: 07H

                   Returned   Value:
                      Register R7: Character
```

This function reads the next character from the currently assigned
auxiliary input device into register R7.  It reports an end-of-file condition
by returning an ASCII CTRL-Z (1AH).

```
                        FUNCTION 8:  HOME

                     Entry Parameters:
                        Register R3: 08H

                     Returned   Value: None
```

This function returns the disk head of the currently selected disk
to the track 00 position.  If your controller does not have a special
feature for finding track 00, you can translate the call to a SETTRK function
with a parameter of 0.

```
                 FUNCTION 9:  SELECT DISK DRIVE

              Entry Parameters:
                 Register R3: 09H
                 Register R5: Disk Drive
                 Register R7: Logged in Flag

              Returned   Value:
                 Register RR6: Address of Selected
                                Drive's DPH
```

This function selects the disk drive specified in register R5 for
further operations.  Register R5 contains 0 for drive A, 1 for drive B,
up to 15 for drive P.

On each disk select, this function returns the address of the selected drive's
Disk Parameter Header in register RR6.  See Section 5 for a discussion of the
Disk Parameter Header.

If there is an attempt to select a nonexistent drive, this function
returns 00000000H
in register RR6 as an error indicator.  Although the function must return the
header address on each call, it may be advisable to postpone the actual
physical disk select operation until an I/O function (seek, read, or
write) is performed.  Disk select operations can occur without a
subsequent disk operation.  Thus, doing a physical select each time this
function is called may be wasteful of time.

On entry to the Select Disk Drive function,
if the least significant bit in register R7 is zero,
the disk is not currently logged in.  If the disk drive is capable of
handling varying media (such as single- and double-sided disks, single- and
double-density, and so on), the BIOS should check the type of
media currently installed and set up the Disk Parameter Block
accordingly at this time.

```
                 FUNCTION 10:  SET TRACK NUMBER

               Entry Parameters:
                  Register R3: 0AH
                  Register R5: Disk track number

               Returned   Value: None
```

This function specifies in register R5 the disk track number for
use in subsequent disk accesses.  The track number remains valid until
either another Function 10 or a Function 8 (Home) is performed.

You can choose to physically seek to the selected track at this time,
or delay the physical seek until the next read or write actually occurs.

The track number can range from 0 to the maximum track number supported
by the physical drive.  However, the maximum track number is limited
to 65535 by the fact that it is being passed as a 16-bit quantity.
Standard floppy disks have tracks numbered from 0 to 76.

```
                 FUNCTION 11:  SET SECTOR NUMBER

                 Entry Parameters:
                    Register R3: 0BH
                    Register R5: Sector Number

                 Returned   Value: None
```

This function specifies in register R5 the sector number for subsequent disk
accesses.  This number remains in effect until either another Function 11 is
performed.

The function selects actual (unskewed) sector numbers.  If skewing is
appropriate, it will have previously been done by a call to Function 16.
You can send this information to the controller at this point or delay sector
selection until a read or write operation occurs.

```
                  FUNCTION 12:  SET DMA ADDRESS

                  Entry Parameters:
                     Register R3: 0CH
                     Register RR4: DMA Address

                  Returned   Value: None
```

This function contains the DMA (disk memory access) address in register
RR4 for subsequent read or write operations.  Note that the controller
need not actually support DMA (direct memory access).  The BIOS will use
the 128-byte area starting at the selected DMA address
for the memory buffer during the following read or write operations.
This function can be called with either
an even or an odd address for a DMA buffer.

```
                    FUNCTION 13:  READ SECTOR

              Entry Parameters:
                 Register R3: 0DH

              Returned   Value:
                 Register R7: 0 if no error
                 Register R7: 1 if physical error
```

After the drive has been selected, the track has been set, the sector has
been set, and the DMA address has been specified, the read function uses
these parameters to read one sector and returns the error code
in register R7.

Currently, CP/M-8000 responds only to a zero or nonzero return code value.
Thus, if the value in register R7 is zero, CP/M-8000 assumes that the disk
operation completed properly.  If an error occurs however, the BIOS should
attempt at least ten retries to see if the error is recoverable.

```
                   FUNCTION 14:  WRITE SECTOR

          Entry Parameters:
             Register R3: 0EH
             Register R5: 0=normal write
                          1=write to a directory
                              sector
                          2=write to first sector
                              of new block

          Returned   Value:
             Register R7: 0=no error
                          1=physical error
```

This function is used to write 128 bytes of data from the currently
selected DMA buffer to the currently selected sector, track, and disk.
The value in register R5 indicates whether the write is an ordinary
write operation or whether the there are special considerations.

If register R5=0, this is an ordinary write operation.  If
R5=1, this is a write to a directory sector, and the write should
be physically completed immediately.  If R5=2, this is a write
to the first sector of a newly allocated block of the disk.  The
significance of this value is discussed in Section 5 under Disk Buffering.

```
                FUNCTION 15:  RETURN LIST STATUS

            Entry Parameters:
               Register R3: 0FH

            Returned   Value:
                 Register R7: 00FFH=device ready
                 Register R7: 0000H=device not ready
```

This function returns the status of the list device.  Register R7
contains either 0000H when the list device is not ready to accept a character
or 00FFH when a character can be sent to the list device.

```
                 FUNCTION 16:  SECTOR TRANSLATE

             Entry Parameters:
                Register R3: 10H
                Register R5: Logical Sector Number
                Register RR6: Address of Translate
                               Table

             Returned   Value:
                Register R7: Physical Sector Number
```

This function performs logical-to-physical sector translation, as discussed
in Section 5.2.2.
The Sector Translate function receives a logical sector
number from register R5.
The logical sector number can range from 0 to the number of sectors
per track-1.
Sector Translate also receives the address of
the translate table in register RR6.  This address must be in the
system's address space.
The logical sector number is used as an index into the translate table.
The resulting physical sector number is returned in R7.

If register RR6 = 00000000H, implying that there is no translate table,
register R5 is copied to register R7 before returning.  Note that
other algorithms are possible; in particular, is is common to increment
the logical sector number in order to convert the logical range of 0 to n-1
into the physical range of 1 to n.
Sector Translate is always called by the BDOS, whether
the translate table address in the Disk Parameter Header is zero or nonzero.

```
               FUNCTION 18:  GET ADDRESS OF MEMORY
                                 REGION TABLE

                 Entry Parameters:
                    Register R3: 12H

                 Returned   Value:
                    Register RR6: Memory Region
                                   Table Address
```

This function returns the address of the Memory Region Table (MRT) in
register RR6.  The MRT describes the segments that compose the TPA
for non-segmented programs.  The format of the MRT is shown below:

```
            Entry Count = 4  16 bits

            Base address of first region     32 bits

            Length of first region           32 bits

            Base address of second region    32 bits

            Length of second region          32 bits

            Base address of third region     32 bits

            Length of third region           32 bits

            Base address of fourth region    32 bits

            Length of fourth region          32 bits
```

##### Figure 4-1.  Memory Region Table Format

The regions are:  1) the segment used for programs with merged program
and data segments;  2) the program segment for programs with split
program and data segments;  3) the data segment for programs with split
program and data segments;  4) a data segment that gives access to
region 2.  (In other words, the segment number in region 2 is the
one that goes into the program counter of a program executing in it,
while the segment number in region 4 is a segment in data space
that allows loading into the program space.)

The memory region table must begin on an even address, and must be implemented.

```
                   FUNCTION 19:  GET I/O BYTE

               Entry Parameters:
                  Register R3: 13H

               Returned   Value:
                  Register R7: I/O Byte Current
                                 Value
```

This function returns the current value of the logical to physical
input/output device byte (I/O byte) in register R7.  This 8-bit
value associates physical devices with CP/M-8000's four logical devices
as noted below.  Note that even though this is a byte value, we are
using word references.  The upper byte should be zero.

Peripheral devices other than
disks are seen by CP/M-8000 as logical devices, and are assigned to
physical devices within the BIOS.  Device characteristics are defined
in Table 4-3 below.

##### Table 4-3.  CP/M-8000 Logical Device Characteristics

| Device Name      | Characteristics                                                                                                                                                          |
| ---------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| CONSOLE          | The interactive console that you use to communicate with the system is accessed through functions 2, 3 and 4.  Typically, the console is a CRT or other terminal device. |
| LIST             | The listing device is a hard-copy device, usually a printer.                                                                                                             |
| AUXILIARY OUTPUT | An optional serial output device.                                                                                                                                        |
| AUXILIARY INPUT  | An optional serial input device.                                                                                                                                         |

Note that a single peripheral can be assigned as the LIST, AUXILIARY INPUT,
and AUXILIARY OUTPUT device simultaneously.  If no peripheral device is
assigned as the LIST, AUXILIARY INPUT, or AUXILIARY OUTPUT device, your BIOS
should give an appropriate error message so that the system does not hang if
the device is accessed by PIP or some other transient program.
Alternatively, the AUXILIARY OUTPUT and LIST functions can simply do nothing
except return to the caller, and the AUXILIARY INPUT function can return with
a 1AH (CTRL-Z) in register R7 to indicate immediate end-of-file.

The I/O byte is split into four 2-bit fields called CONSOLE, AUXILIARY INPUT,
AUXILIARY OUTPUT, and LIST, as shown in Figure 4-2.

```
        Most Significant		Least Significant

  I/O Byte   LIST   AUXILIARY OUTPUT   AUXILIARY INPUT  CONSOLE

   bits:      7,6           5,4               3,2          1,0
```

##### Figure 4-2.  I/O Byte Fields

The value in each field can be in the range 0-3, defining the assigned
source or destination of each logical device.  The values which can
be assigned to each field are given in Table 4-4.

##### Table 4-4.  I/O Byte Field Definitions

CONSOLE field (bits 1,0)

| Bit | Definition                                                                                                 |
| --- | ---------------------------------------------------------------------------------------------------------- |
| 0   | console is assigned to the console printer (TTY:)                                                          |
| 1   | console is assigned to the CRT device (CRT:)                                                               |
| 2   | batch mode: use the AUXILIARY INPUT as the CONSOLE input, and the LIST device as the CONSOLE output (BAT:) |
| 3   | user defined console device (UC1:)                                                                         |

AUXILIARY INPUT field (bits 3,2)

| Bit | Definition                                             |
| --- | ------------------------------------------------------ |
| 0   | AUXILIARY INPUT is the Teletype device (TTY:)          |
| 1   | AUXILIARY INPUT is the high-speed reader device (PTR:) |
| 2   | user defined reader #1 (UR1:)                          |
| 3   | user defined reader #2 (UR2:)                          |

AUXILIARY OUTPUT field (bits 5,4)

| Bit | Definition                                             |
| --- | ------------------------------------------------------ |
| 0   | AUXILIARY OUTPUT is the Teletype device (TTY:)         |
| 1   | AUXILIARY OUTPUT is the high-speed punch device (PTP:) |
| 2   | user defined punch #1 (UP1:)                           |
| 3   | user defined punch #2 (UP2:)                           |

LIST field (bits 7,6)

| Bit | Definition                             |
| --- | -------------------------------------- |
| 0   | LIST is the Teletype device (TTY:)     |
| 1   | LIST is the CRT device (CRT:)          |
| 2   | LIST is the line printer device (LPT:) |
| 3   | user defined list device (UL1:)        |

Note that the implementation of the I/O byte is optional, and affects
only the organization of your BIOS.  No CP/M-8000 utilities use the
I/O byte except for PIP, which allows access to the physical devices, and
STAT, which allows logical-physical assignments to be made and displayed.
It is a good idea to first implement and test your BIOS without the IOBYTE
functions, then add the I/O byte function.

```
                   FUNCTION 20:  SET I/O BYTE

                    Entry Parameters:
                       Register R3: 14H
                       Register R5: Desired

                    Returned   Value: None
```

This function uses the value in register R5 to set the value of the
I/O byte that is stored in the BIOS.  See Table 4-4 for the
I/O byte field definitions.  Note that even though this is a byte
value, we are using word references.  The upper byte should be zero.

```
                   FUNCTION 21:  FLUSH BUFFERS

           Entry Parameters:
              Register R3: 15H

           Returned   Value:
              Register R7: 0000H=successful write
              Register R7: FFFFH=unsuccessful write
```

This function forces the contents of any disk buffers that have been
modified to be written.  That is, after this function has
been performed, all disk writes have been physically completed.  After the
buffers are written, this function returns a zero in register R7.
However, if the buffers cannot be written or an error occurs, the function
returns a value of FFFFH in register R7.

```
           FUNCTION 22:  SET EXCEPTION HANDLER ADDRESS

            Entry Parameters:
               Register R3:  16H
               Register R5:  Exception Vector Number
               Register RR6: Exception Vector Address

            Returned   Value:
               Register RR6: Previous Vector Contents
```

This function sets the exception vector indicated in register R5
to the value specified in register RR6.  The previous vector value
is returned in register RR6.  Unlike the BDOS Set Exception Vector
Function (61), this BIOS function sets any exception vector.
Note that register R5 contains the exception vector number.  Thus,
to set exception #2, segmentation trap, this register contains a 2.

The exception handler is called as a subroutine, with all of
its registers saved on the stack, in the form given for
the context block in the Transfer Control instruction.
On a segmented CPU, the
exception handler is enterred in segmented mode.  It should
return with a RET instruction.

All of the caller's registers except RR0 are also passed
intact to the handler.

### 4.1  Memory Management

The system call SC #1 is used for several memory-management
operations:  mapping addresses from logical to physical, copying
blocks of (physical) memory, and transferring control from
one address space to another.  Parameters are specified in
registers RR2, RR4, and RR6, and a value may be returned in
RR6.

For this operation it is necessary to distinguish between
logical and physical addresses.  A logical address refers to
an address in a program's address space; it is 16 bits long
for a non-segmented program, and 23 bits long (stored in a
32-bit word) in a segmented program.  The hardware may perform
some mapping on a logical address, turning it into a physical
address.  Also, when a non-segmented program is running on
a segmented CPU, the non-segmented logical address acquires
a segment number (taken from the PC), which becomes part of
the logical address.

For the purposes of CPM-8000, it is necessary that the logical-to-physical
mapping process not affect the low-order 16 bits (offset part)
of an address.  Thus, on some systems (for example, those with
MMU's that permit segments to start on arbitrary boundaries)
the "physical" addresses used inside of the BIOS might undergo
further mapping.  It is only necessary that the BIOS's physical
addresses be able to distinguish between all memory segments
belonging to the system and to the TPA.

All BIOS operations done through SC #3 expect full 32-bit
physical addresses.  BIOS operations done through BDOS call 50
are mapped from the caller's address space into physical addresses.

```
           SYSTEM CALL 1:  MEMORY COPY

            Entry Parameters:
               Register RR2: Length
               Register RR4: Destination
               Register RR6: Source

            Returned   Value:  None
```

This operation copies a block of Length bytes from Source to Destination.
Length must be greater than zero and less than 65536 (a Length of zero
is used to distinguish different memory management operations).  The
Source and Destination are segmented physical addresses, as provided
by the Map Address operation (below).

```
           SYSTEM CALL 1:  MAP ADDRESS

            Entry Parameters:
               Register RR2: 0
               Register RR4: Space Code
               Register RR6: Logical Address

            Returned   Value:
               Register RR6: Physical Address
```

This form of SC #1 is used to convert a logical address to a
physical address.  Since logical addresses depend on both the
mode (system or normal) of the program using them, and on the
space being accessed (program or data), a code is used to
determine which space to map from.

If the program in the TPA is running non-segmented, the
Set TPA Segment version of SC #1 will have been used to tell
the mapping routine which segment is being used.  If the TPA
is running with split program and data, it is also necessary
to distinguish between the segment number that goes in the
program counter to access instructions, and the physical segment
by which the TPA's instruction segment can be accessed as data.

The space codes are as follows:

```
	0:	Caller's Data Space
	1:	Caller's Program Space (as Data)
	257:	Caller's Program Space (as Instructions)
	2:	System's Data Space
	3:	System's Program Space (as Data)
	259:	System's Program Space (as Instructions)
	4:	TPA's Data Space
	5:	TPA's Program Space
	261:	TPA's Program Space (as Instructions)
```

```
           SYSTEM CALL 1:  SET TPA SEGMENT

            Entry Parameters:
               Register RR2: 0
               Register RR4: 0000FFFFh
               Register RR6: TPA Base Address

            Returned   Value:  None
```

This operation sets the base segment for a non-segmented program
running in the TPA.  This base address is usually obtained from
entry 1 in the Memory Region Table for programs with programs
and data in the same segment, and from entry 2 for programs with
split program and data segments.

If R6 (the high-order word of RR6) is FFFFh, the program running
in the TPA will be assumed to be running in segmented mode.

```
           SYSTEM CALL 1:  TRANSFER CONTROL

            Entry Parameters:
               Register RR2: 0
               Register RR4: FFFEh
               Register RR6: Context Block Address

            Returned   Value:  none
```

This operation causes control to be transferred to another address
space.  It allows all of the registers to be specified (except for
the system mode stack pointer), and is used by the debugger to
transfer control to the program being debugged.  RR6 points to
a context block of the form:

```
		word	R0
		word	R1
		word	R2
		word	R3
		word	R4
		word	R5
		word	R6
		word	R7
		word	R8
		word	R9
		word	R10
		word	R11
		word	R12
		word	R13
		word	R14 (normal mode R14)
		word	R15 (normal mode R14)
		word	ignored
		word	FCW (Flag/Control Word)
		word	PC Segment
		word	PC Offset
```

Note that the PC segment word is required even if the CPU is
a non-segmented Z8002, for compatibility reasons.
End of Section 4

## Section 5

### Creating a BIOS

### 5.1  Overview

The BIOS provides a standard interface to the physical input/output devices
in your system.  The BIOS interface is defined by the functions
described in Section 4.  Those functions, taken together, constitute a model
of the hardware environment.  Each BIOS is responsible for mapping that
model onto the real hardware.

In addition, the BIOS contains disk definition tables which define
the characteristics of the
disk devices which are present, and provides some storage for use by the BDOS
in maintaining disk directory information.

Section 4 describes the functions which must be performed by the BIOS, and the
external interface to those functions.  This Section contains additional
information describing the structure and significance of the disk definition
tables and information about sector blocking and deblocking.  Careful choices
of disk parameters and disk buffering methods are necessary if you are to
achieve the best possible performance from CP/M-8000.  Therefore, this section
should be read thoroughly before writing a custom BIOS.

### 5.2  Disk Definition Tables

As in other CP/M systems, CP/M-8000 uses a set of tables to define disk
device characteristics.  This section describes each of these tables
and discusses choices of certain parameters.

#### 5.2.1  Disk Parameter Header

Each disk drive has an associated 26-byte Disk Parameter Header (DPH)
which both contains information about the disk drive and
provides a scratchpad area for certain BDOS operations.  Each drive must
have its own unique DPH.  The format of a Disk Parameter Header is shown in
Figure 5-1.

```
     XLT    0000   0000   0000  DIRBUF   DPB     CSV     ALV

     32b     16b    16b    16b    32b    32b     32b     32b
```

##### Figure 5-1.  Disk Parameter Header

Each element of the DPH is either a word (16-bit) or longword (32-bit)
value.  The meanings of the Disk Parameter Header (DPH) elements are given
in Table 5-1.

##### Table 5-1.  Disk Parameter Header Elements

| Element | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| ------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| XLT     | Address of the logical-to-physical sector translation table, if used for this particular drive, or the value 0 if there is no translation table for this drive (i.e, the physical and logical sector numbers are the same). Disk drives with identical sector translation may share the same translate table.  The sector translation table is described in Section 5.2.2.                                                                                                                                                          |
| 0000    | Three scratchpad words for use within the BDOS.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| DIRBUF  | Address of a 128-byte scratchpad area for directory operations within BDOS.  All DPHs address the same scratchpad area.                                                                                                                                                                                                                                                                                                                                                                                                             |
| DPB     | Address of a disk parameter block for this drive.  Drives with identical disk characteristics may address the same disk parameter block.                                                                                                                                                                                                                                                                                                                                                                                            |
| CSV     | Address of a checksum vector.  The BDOS uses this area to maintain a vector of directory checksums for the disk.  These checksums are used in detecting when the disk in a drive has been changed.  If the disk is not removable, then it is not necessary to have a checksum vector.  Each DPH must point to a unique checksum vector.  The checksum vector should contain 1 byte for every four directory entries (or 128 bytes of directory).  In other words: length (CSV) = (DRM+1) / 4.  (DRM is discussed in Section 5.2.3.) |
| ALV     | Address of a scratchpad area used by the BDOS to keep disk storage allocation information.  The area must be different for each DPH.  There must be 1 bit for each allocation block on the drive, requiring the following: length (ALV) = (DSM/8) + 1.  (DSM is discussed below.)                                                                                                                                                                                                                                                   |

#### 5.2.2  Sector Translate Table

Sector translation in CP/M-8000 is a method of logically renumbering
the sectors on each disk track to improve disk I/O performance.  A frequent
situation is that a program needs to access disk sectors
sequentially.  However, in reading sectors sequentially, most programs
lose a full disk revolution between sectors because there is not
enough time between adjacent sectors to begin a new disk operation.  To
alleviate this problem, the traditional CP/M solution is to create a logical
sector numbering scheme in which logically sequential sectors are physically
separated.  Thus, between two logically contiguous sectors, there is a
several sector rotational delay.  The sector translate table defines
the logical-to-physical mapping in use for a particular drive, if a
mapping is used.

Sector translate tables are used only within the BIOS.  Thus the table
may have any convenient format.  (Although the BDOS is aware of the
sector translate table, its only interaction with the table is to get the
address of the sector translate table from the DPH and to pass that address
to the Sector Translate Function of the BIOS.)  The most common form for a
sector translate table is an n-byte (or n-word) array of physical sector
numbers, where n is the number of sectors per disk track.  Indexing into
the table with the logical sector number yields the corresponding physical
sector number.

Although you may choose any convenient logical-to-physical mapping, there is
a nearly universal mapping used in the CP/M community for single-sided,
single-density, 8-inch diskettes.  That mapping is shown in Figure 5-2.
Because your choice of mapping affects diskette compatibility between
different systems, the mapping of Figure 5-2 is strongly recommended.

```
     Logical  Sector   0  1  2  3  4  5  6  7  8  9 10 11 12
     Physical Sector   1  7 13 19 25  5 11 17 23  3  9 15 21

     Logical  Sector  13 14 15 16 17 18 19 20 21 22 23 24 25
     Physical Sector   2  8 14 20 26  6 12 18 24  4 10 16 22
```

##### Figure 5-2.  Sample Sector Translate Table

#### 5.2.3  Disk Parameter Block

A Disk Parameter Block (DPB) defines several characteristics associated
with a particular disk drive.  Among them are the size of the drive, the
number of sectors per track, the amount of directory space, and others.

A Disk Parameter Block can be used in one or more DPH's if the disks are
identical in definition.  A discussion of the fields of the DPB follows the
format description.  The format of the DPB is shown in Figure 5-3.

```
  SPT   BSH   BLM   EXM   0   DSM   DRM   Reserved    CKS   OFF

  16b   8b     8b    8b   8b  16b   16b      16b      16b   16b
```

##### Figure 5-3.  Disk Parameter Block

Each field is a word (16 bit) or a byte (8 bit) value.  The description of
each field is given in Table 5-2.

##### Table 5-2.  Disk Parameter Block Fields

| Field | Definition                                                                                                                                                                                                                                                                                                                                                                                         |
| ----- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SPT   | Number of 128-byte logical sectors per track.                                                                                                                                                                                                                                                                                                                                                      |
| BSH   | The block shift factor, determined by the data block allocation size, as shown in Table 5-3.                                                                                                                                                                                                                                                                                                       |
| BLM   | The block mask which is determined by the data block allocation size, as shown in Table 5-3.                                                                                                                                                                                                                                                                                                       |
| EXM   | The extent mask, determined by the data block allocation size and the number of disk blocks, as shown in Table 5-4.                                                                                                                                                                                                                                                                                |
| 0     | Reserved byte.                                                                                                                                                                                                                                                                                                                                                                                     |
| DSM   | Determines the total storage capacity of the disk drive and is the number of the last block, counting from 0.  That is, the disk contains DSM+1 blocks.                                                                                                                                                                                                                                            |
| DRM   | Determines the total number of directory entries which can be stored on this drive.  DRM is the number of the last directory entry, counting from 0.  That is, the disk contains DRM+1 directory entries. Each directory entry requires 32 bytes, and for maximum efficiency, the value of DRM should be chosen so that the directory entries exactly fill an integral number of allocation units. |
| CKS   | The size of the directory check vector, which is zero if the disk is permanently mounted, or length (CSV) = (DRM) / 4 + 1 for removable media.                                                                                                                                                                                                                                                     |
| OFF   | The number of reserved tracks at the beginning of a logical disk.  This is the number of the track on which the directory begins.                                                                                                                                                                                                                                                                  |

To choose appropriate values for the Disk Parameter Block elements, you must
understand how disk space is organized in CP/M-8000.  A CP/M-8000
disk has two major areas:  the boot or system tracks, and the file system
tracks.  The boot tracks are usually used to hold a machine-dependent
bootstrap loader for the operating system.  They consist of tracks 0 to OFF-1.
Zero is a legal value for OFF, and in that case, there are no boot tracks.  The
usual value of OFF for 8-inch floppy disks is two.

The tracks after the boot tracks (beginning with track number OFF)
are used for the disk directory and disk files.  Disk space in this area
is grouped into units called allocation units or blocks.  The block size
for a particular disk is a constant, called BLS.  BLS may take on any
one of these values: 1024, 2048, 4096, 8192, or 16384 bytes.  No other
values for BLS are allowed.  (Note that BLS does not appear explicitly in
any BIOS table.  However, it determines the values of a number of other
parameters.)  The DSM field in the Disk Parameter Block is one less than
the number of blocks on the disk.  Space is allocated to a file
or to the directory in
whole blocks.  No fraction of a block can be allocated.
block size

The choice of BLS is very important, because it effects the efficiency of
disk space utilization, and because for any disk size there is a minimum
value of BLS that allows the entire disk to be used.
Each block on the disk has a block number ranging from 0 to DSM.  The largest
block number allowed is 32767.  Therefore, the largest number of bytes that
can be addressed in the file system space is 32768 * BLS.  Because the largest
allowable value for BLS is 16384, the biggest disk that can be accessed by
CP/M-8000 is 16384*32768 = 512 Mbytes.

Each directory entry may contain either 8 block numbers (if DSM >= 256)
or 16 block
numbers (if DSM < 256).  Each file needs enough directory entries to hold
the block numbers of all blocks allocated to the file.  Thus a large value
for BLS implies that fewer directory entries are needed.  Since fewer
directory entries are used, the directory search time is decreased.

The disadvantage of a large value for BLS is that since files are allocated
BLS bytes at a time, there is potentially a large unused portion of a block
at the end of the file.  If there are many small files on a disk, the waste
can be very significant.

The BSH and BLM parameters in the DPB are functions of BLS.  Once you have
chosen BLS, you should use Table 5-3 to determine BSH and BLM.  The EXM
parameter of the DPB is a function of BLS and DSM.  You should use Table 5-4
to find the value of EXM for your disk.

##### Table 5-3.   BSH and BLM Values

| BLS   | BSH | BLM |
| ----- | --- | --- |
| 1024  | 3   | 7   |
| 2048  | 4   | 15  |
| 4096  | 5   | 31  |
| 8192  | 6   | 63  |
| 16384 | 7   | 127 |

##### Table 5-4.  EXM Values

| BLS   | DSM <= 255 | DSM > 255 |
| ----- | ---------- | --------- |
| 1024  | 0          | N/A       |
| 2048  | 1          | 0         |
| 4096  | 3          | 1         |
| 8192  | 7          | 3         |
| 16384 | 15         | 7         |

The DRM entry in the DPB is one less than the total number of directory
entries.  DRM should be chosen large enough so that you do not run
out of directory entries before running out of disk space.  It is not
possible to give an exact rule for determining DRM, since the number
of directory entries needed will depend on the number and sizes of the
files present on the disk.

The CKS entry in the DPB is the number of bytes in the CSV (checksum vector)
which was pointed to by the DPH.  If the disk is not removable, a checksum
vector is not needed, and this value may be zero.

### 5.3  Disk Blocking System Guide

When the BDOS does a disk read or write operation using the BIOS, the unit
of information read or written is a 128-byte sector.  This may or may not
correspond to the actual physical sector size of the disk.  If not, the BIOS
must implement a method of representing the 128-byte sectors used by CP/M-8000
on the actual device.  Usually if the physical sectors are not 128 bytes long,
they will be some multiple of 128 bytes.  Thus, one physical sector can hold
some integer number of 128-byte CP/M sectors.  In this case, any disk I/O
will actually consist of transferring several CP/M sectors at once.

It might also be desirable to do disk I/O in units of several 128-byte sectors
in order to increase disk throughput by decreasing rotational latency.
(Rotational latency is the average time it takes for the desired position on
a disk to rotate around to the read/write head.  Generally this averages 1/2
disk revolution per transfer.)  Since a great deal of disk I/O is sequential,
rotational latency can be greatly reduced by reading several sectors at a
time, and saving them for future use.

In both the cases above, the point of interest is that physical I/O
occurs in units larger than the expected sector size of 128 bytes.
Some of the problems in doing disk I/O in this manner are discussed below.

#### 5.3.1  A Simple Approach

This section presents a simple approach to handling a physical sector size
larger than the logical sector size.  The method discussed in this section
is not recommended for use in a real BIOS.  Rather, it is given as a
starting point for refinements discussed in the following sections.
Its simplicity also makes it a logical choice for a first BIOS on new
hardware.  However, the disk throughput that you can achieve with this method
is poor, and the refinements discussed later give dramatic improvements.

Probably the easiest method for handling a physical sector size which
is a multiple of 128 bytes is to have a single buffer the size of the
physical sector internal to the BIOS.  Then, when a disk read is to be
done, the physical sector containing the desired 128-byte logical sector
is read into the buffer, and the appropriate 128 bytes are copied to the
DMA address.  Writing is a little more complicated.  You only want to
put data into a 128-byte portion of the physical sector, but you can
only write a whole physical sector.  Therefore, you must first read the
physical sector into the BIOS's buffer; copy the 128 bytes of output data
into the proper 128-byte piece of the physical sector in the buffer; and
finally write the entire physical sector back to disk.

#### Note:

this operation involves two rotational latency delays in addition to the time
needed to copy the 128 bytes of data.  In fact, the second rotational wait
is probably nearly a full disk revolution, since the copying is usually
much faster than a disk revolution.

#### 5.3.2  Some Refinements

There are some easy things that can be done to the algorithm of Section 5.2.1
to improve its performance.  The first is based on the fact that disk
accesses are usually done sequentially.  Thus, if data from a certain physical
sector is needed, it is likely that another piece of that sector will be
needed on the next disk operation.  To take advantage of this fact, the BIOS
can keep information with its physical sector buffer as to which disk,
track, and physical sector (if any) is represented in the buffer.  Then, when
reading, the BIOS need only do physical disk reads when the information needed
is not in the buffer.

On writes, the BIOS still needs to preread the physical sector for the same
reasons discussed in Section 5.2.1, but once the physical sector is in the
buffer, subsequent writes into that physical sector do not require additional
prereads.  An additional saving of disk accesses can be gained by not
writing the sector to the disk until absolutely necessary.  The conditions
under which the physical sector must be written are discussed in
Section 5.3.4.

#### 5.3.3  Track Buffering

Track buffering is a special case of disk buffering where the I/O is done
a full track at a time.  When sufficient memory for several full track
buffers is available, this method is quite good.  The method is essentially
the same as discussed in Section 5.3.2, but there are some interesting
features.  First, transferring an entire track is much more efficient than
transferring a single sector.  The rotational latency is incurred only once
for the entire track, whereas if the track is transferred one sector at a
time, the rotational latency occurs once per sector.  On a typical diskette
with 26 sectors per track, rotating at 6 revolutions per second, the difference
in rotational latency per track is about 2 seconds versus a twelfth of a
second.  Of course, in applications where the disk is accessed purely
randomly, there is no advantage because there is a low probability that
more than one sector will be used from a given track.  However, such
applications are extremely rare.

#### 5.3.4  LRU Replacement

With any method of disk buffering using more than one buffer, it is necessary
to have some algorithm for managing the buffers.  That is, when should buffers
be filled, and when should they be written back to disk.  The first question
is simple, a buffer should be filled when there is a request for a disk sector
that is not presently in memory.  The second issue, when to write a buffer
back to disk, is more complicated.

Generally, it is desirable to defer writing a buffer until it becomes
necessary.  Thus, several transfers can be done to a buffer for the cost of
only one disk access, two accesses if the buffer had to be preread.
However, there are several reasons why buffers must be written.  The following
list describes the reasons:

1) A BIOS Write operation with mode=1 (write to directory sector).  To maintain
the integrity of CP/M-8000's file system, it is very important that directory
information on the disk is kept up to date.  Therefore, all directory writes
should be performed immediately.

2) A BIOS Flush Buffers operation.  This BIOS function is explicitly intended
to force all disk buffers to be written.  After performing a Flush Buffers,
it is safe to remove a disk from its drive.

3) A disk buffer is needed, but all buffers are full.  Therefore some
buffer must be emptied to make it available for reuse.

4) A Warm Boot occurs.  This is similar to number 2 above.

Case three above is the only one in which the BIOS writer has any discretion
as to which buffer should be written.  Probably the best strategy is to
write out the buffer which has been least recently used.  The fact that
an area of disk has not been accessed for some time is a fairly good
indication that it will not be needed again soon.

#### 5.3.5  The New Block Flag

As explained in Section 5.2.2, the BDOS allocates disk space to files
in blocks of BLS bytes.  When such a block is first allocated to a file,
the information previously in that block need not be preserved.  To
enable the BIOS to take advantage of this fact, the BDOS uses a special
parameter in calling the BIOS Write Function.  If register D1.W contains
the value 2 on a BIOS Write call, then the write being done is to the
first sector of a newly allocated disk block.  Therefore, the BIOS need
not preread any sector of that block.  If the BIOS does disk buffering
in units of BLS bytes, it can simply mark any free buffer as corresponding
to the disk address specified in this write, because the contents of the
newly allocated block are not important.  If the BIOS uses a buffer size
other than BLS, then the algorithm for taking full advantage of this
information is more complicated.

This information is extremely valuable in reducing disk delays.  Consider
the case where one file is read sequentially and copied to a newly created
file.  Without the information about newly allocated disk blocks, every
physical write would require a preread.  With the information,
no physical write requires a preread.  Thus, the number of physical disk
operations is reduced by one third.

End of Section 5

## Section 6

### Installing and Adapting the Distributed BIOS and CP/M-8000

### 6.1  Overview

The process of bringing up your first running CP/M-8000 system is
either trivial or involved, depending on your hardware environment.
Digital Research supplies CP/M-8000 in a form suitable for booting
on a Zilog EXORmacs development system.  If you have an EXORmacs,
you can read Section 6.1 which tells how to load the distributed system.
Similarly, you can buy or lease some other machine which already runs
CP/M-8000.

If you do not have an EXORmacs, you can use the S-record files supplied
with your distribution disks to bring up your first CP/M-8000 system.
This process is discussed in Section 6.2.

### 6.2  Booting on an EXORmacs

The CP/M-8000 disk set distributed by Digital Research includes disks
boot and run CP/M-8000 on the Zilog EXORmacs.  You can use the distribution
system boot disk without modification if you have a Zilog EXORmacs system
and the following configuration:

1) 128K memory (minimum)

2) a Universal Disk Controller (UDC) or Floppy Disk Controller (FDC)

3) a single-density, IBM 3740 compatible floppy disk drive

4) an EXORterm

To load CP/M-8000, do the following:

1) Place the disk in the first floppy drive (#FD04 with the UDC or #FD00
with the FDC).

2) Press SYSTEM RESET (front panel) and RETURN (this brings in MACSbug).

3) Type "BO 4" if you are using the UDC, "BO 0" if you are using the
FDC, and RETURN.  CP/M-8000 boots and begins running.

### 6.3  Bringing Up CP/M-8000 Using the S-record Files

The CP/M-8000 distribution disks contain two copies of the CP/M-8000 operating
system in Zilog S-record form, for use in getting your first CP/M-8000
system running.  S-records (described in detail in Appendix F) are a simple
ASCII representation for absolute programs.  The two S-record
systems contain the CCP and BDOS, but no BIOS.  One of the S-record systems
resides at locations 400H and up, the other is configured to occupy the top
of a 128K memory space.  (The exact bounds of the S-record systems may vary
from release to release.  There will be release notes and/or a file named
README describing the exact characteristics of the S-record systems
distributed on your disks.)  To bring up CP/M-8000 using the S-record files,
you need:

1) some method of down-loading absolute data into your target system

2) a computer capable of reading the distribution disks (a CP/M-based
computer that supports standard CP/M 8-inch diskettes)

3) a BIOS for your target computer

Given the above items, you can use the following procedure to bring a working
version of CP/M-8000 into your target system:

1) You must patch one location in the S-record system to link it to your BIOS's
_init entry point.  This location will be specified in release notes and/or
in a README file on your distribution disks.  The patch simply consists of
inserting the address of the _init entry in your BIOS at one long word
location in the S-record system.  This patching can be done either before
or after down-loading the system, whichever is more convenient.

2) Your BIOS needs the address of the _ccp entry point in the S-record
system.  This can be obtained from the release notes and/or the README file.

3) Down-load the S-record system into the memory of your target computer.

4) Down-load your BIOS into the memory of your target computer.

5) Begin executing instructions at the first location of the down-loaded
S-record system.

Now that you have a working version of CP/M-8000, you can use the tools
provided with the distribution system for further development.

End of Section 6

## Section 7

### Cold Boot Automatic Command Execution

### 7.1  Overview

The Cold Boot Automatic Command Execution feature of CP/M-8000 allows you to
configure CP/M-8000 so that the CCP will automatically execute a predetermined
command line on cold boot.  This feature can be used to start up turn-key
systems, or to perform other desired operations.

### 7.2  Setting up Cold Boot Automatic Command Execution

The CBACE feature uses two global symbols: _autost, and _usercmd.  These are
both defined in the CCP, which uses them on cold boot
to determine whether this feature is enabled.  If you want to have a CCP
command automatically executed on cold boot, you should include code in your
BIOS's _init routine (which is called at cold boot) to do the following:

1) The byte at _autost must be set to the value 01H.

2) The command line to be executed must be placed in memory at _usercmd and
subsequent locations.  The command must be terminated with a NULL (00H) byte,
and may not exceed 128 bytes in length.  All alphabetic characters in the
command line should be upper-case.

Once you write a BIOS that performs these two functions, you can
build it into a CPM.SYS file as described in Section 2.  This system, when
booted, will execute the command you have built into it.

End of Section 7

## Section 8

### The PUTBOOT Utility

### 8.1  PUTBOOT Operation

The PUTBOOT utility is used to copy information (usually a bootstrap loader
system) onto the system tracks of a disk.  Although PUTBOOT can copy any
file to the system tracks, usually the file being written is a program
(the bootstrap system).

### 8.2  Invoking PUTBOOT

Invoke PUTBOOT with a command of the form:

PUTBOOT [-H] <filename> <drive>

where

o -H is an optional flag discussed below;

o <filename> is the name of the file to be written to the system tracks;

o <drive> is the drive specifier for the drive to which <filename> is to be
written (letter in the range A-P.)

PUTBOOT writes the specified file to the system tracks of the specified
drive.  Sector skewing is not used; the file is written to the system tracks
in physical sector number order.

Because the file that is written is normally in command file format,
PUTBOOT contains special logic to strip off the first 28 bytes of the file
whenever the file begins with the number 601AH, the magic number used in
command files.  If, by chance, the file to be written begins with 601AH, but
should not have its first 28 bytes discarded, the -H flag should be specified
in the PUTBOOT command line.  This flag tells PUTBOOT to write the file
verbatim to the system tracks.

PUTBOOT uses BDOS calls to read <filename>, and used BIOS calls to
write <filename> to the system tracks.  It refers to the OFF and SPT
parameters in the Disk Parameter Block to determine how large the system
track space is.  The source and command files for PUTBOOT are supplied
on the distribution disks for CP/M-8000.

End of Section 8
