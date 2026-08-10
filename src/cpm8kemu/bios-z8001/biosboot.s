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
	.extern	bdossc

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

	call	trapinit	! initialize trap system (no bdossc yet)

	! Install our smart SC #2 dispatcher into the trap vector.
	!
	! Note we do NOT call _bdosini here. This mirrors the real M20 cold boot
	! (src/cpm8k/biosif.8kn), which calls _trapinit + _biosinit and then
	! "jp ccp" -- it never calls _bdosini directly. The CCP cold path calls
	! _bdosini exactly once itself (guarded by its own cold/warm flag), which
	! is what prints the sign-on banner. Calling _bdosini here too made the
	! banner (and the whole BDOS init) run twice. The emulator has no M20 C
	! BIOS, so there is no _biosinit to call; the BIOS is initialised on the
	! host side (bios_init_disks) before the CPU starts.
	ld	r2, _sysseg
	lda	r3, bdossc
	ldl	_trapvec + (BDOS_SC + SC0TRAP) * 4, rr2

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
