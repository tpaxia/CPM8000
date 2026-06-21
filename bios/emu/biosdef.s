! biosdef.s - Shared definitions for CP/M-8000 emulator BIOS
! Subset of bios/biosdef.s needed for the thin emulator BIOS

	.equ	XFER_SC, 1
	.equ	MEM_SC, 1
	.equ	BIOS_SC, 3
	.equ	BDOS_SC, 2

	.equ	NTRAPS, 48
	.equ	SC0TRAP, 32

! I/O ports for C++ handler bridging
	.equ	PORT_BDOS, 0xF0
	.equ	PORT_BIOS, 0xF2
	.equ	PORT_MAP, 0xF4
	.equ	PORT_MEMCPY, 0xF6

	.equ	NMITRAP, 0
	.equ	EPUTRAP, 1
	.equ	SEGTRAP, 2
	.equ	PITRAP, 8
	.equ	TRACETR, 9

	.equ	PCSIZE, 2
	.equ	INTSIZE, 2
	.equ	LONGSIZE, 4

	.equ	ARG1, PCSIZE
	.equ	ARG2, ARG1+INTSIZE
	.equ	ARG3, ARG2+INTSIZE
	.equ	ARG4, ARG3+INTSIZE
	.equ	ARG5, ARG4+INTSIZE

! Stack frame equates for trap handler
	.equ	cr0, 0
	.equ	cr1, cr0+2
	.equ	cr2, cr1+2
	.equ	cr3, cr2+2
	.equ	cr4, cr3+2
	.equ	cr5, cr4+2
	.equ	cr6, cr5+2
	.equ	cr7, cr6+2
	.equ	cr8, cr7+2
	.equ	cr9, cr8+2
	.equ	cr10, cr9+2
	.equ	cr11, cr10+2
	.equ	cr12, cr11+2
	.equ	cr13, cr12+2
	.equ	nr14, cr13+2
	.equ	nr15, nr14+2
	.equ	scinst, nr15+2
	.equ	scfcw, scinst+2
	.equ	scseg, scfcw+2
	.equ	scpc, scseg+2
	.equ	FRAMESZ, scpc+2

! Segmented mode macros
	.macro SEG
	ldctl	r0, fcw
	set	r0, #15
	ldctl	FCW, r0
	.endm

	.macro NONSEG
	ldctl	r0, fcw
	res	r0, #15
	ldctl	FCW, r0
	.endm
