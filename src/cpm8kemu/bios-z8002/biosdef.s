! Shared definitions for the hosted non-segmented Z8002 BIOS.

	.equ	XFER_SC, 1
	.equ	MEM_SC, 1
	.equ	BIOS_SC, 3
	.equ	BDOS_SC, 2
	.equ	NTRAPS, 48
	.equ	SC0TRAP, 32

	.equ	PORT_BDOS, 0xF0
	.equ	PORT_BIOS, 0xF2
	.equ	PORT_MAP, 0xF4
	.equ	PORT_MEMCPY, 0xF6

	.equ	NMITRAP, 0
	.equ	EPUTRAP, 1
	.equ	SEGTRAP, 2
	.equ	PITRAP, 8

	.equ	ARG1, 2
	.equ	ARG2, 4
	.equ	ARG3, 6
	.equ	ARG4, 8
	.equ	ARG5, 10

! Z8002 system-call frame: r0-r14, normal r15, SC, FCW, PC.
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
	.equ	cr14, cr13+2
	.equ	nr15, cr14+2
	.equ	scinst, nr15+2
	.equ	scfcw, scinst+2
	.equ	scpc, scfcw+2
	.equ	FRAMESZ, scpc+2
