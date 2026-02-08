! biosboot.s - Minimal bootstrap for CP/M-8000 emulator
! Entry point: receives control in segmented system mode from emulator.
! Sets up stack, PSA, goes non-segmented, initializes traps, jumps to CCP.

	.include "biosdef.s"

	sect	.text
	unsegm

	.global	entry, _start
	.extern	ccp
	.extern	_bss_top, _bss_end
	.extern	trapinit

	.equ	SYSTEM, 0x0B000000
	.equ	SYSSTK, (SYSTEM + 0x0BFFE)

_start:
entry:
	! We arrive in segmented system mode
	di	vi, nvi

	! Clear BSS region
	ldl	rr2, #_bss_top
	ldl	rr4, #_bss_end
	sub	r5, r3
	inc	r5, #1
1:
	clrb	@r2		! clrb @rr2 (non-seg uses r2)
	inc	r3, #1
	djnz	r5, 1b

	! Set system stack pointer
	ldl	rr14, #SYSSTK

	! Set PSAP to segment 0x02, offset 0x100
	ldl	rr2, #psa
	ldctl	psapseg, r2
	ldctl	psapoff, r3

	! Get our segment number and jump to bios init
	ldar	r2, .
	ld	r3, #bios
	jp	@r2

! BIOS initialization (entered segmented, jumps non-seg)
bios:
	di	vi, nvi
	calr	kludge
kludge:
	popl	rr4, @r14	! get PC segment on stack

	ldctl	r2, PSAPSEG
	ldctl	r3, PSAPOFF

	NONSEG
	ldl	_psap, rr2
	ld	_sysseg, r4
	ld	r14, _sysseg
	ldl	_sysstk, rr14

	push	@r15, #_wboot	! return address = warm boot

	call	trapinit	! initialize trap system

	jp	ccp		! start CCP

! Warm boot
	.global	_wboot
_wboot:
	ldl	rr14, _sysstk
	jp	ccp

!----------------------------------------------------------------------
! Data

	.global	_sysseg, _usrseg, _sysstk, _psap, _trapvec

	sect .bss
	.even
_sysseg:	.space 2
_usrseg:	.space 2
_sysstk:	.space 4
_psap:		.space 4
_trapvec:	.space NTRAPS * 4
