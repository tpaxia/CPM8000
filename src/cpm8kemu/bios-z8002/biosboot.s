! Cold/warm entry for the hosted non-segmented Z8002 CP/M system.

	.include "biosdef.s"

	sect .text
	unsegm

	.global _start, entry, _wboot
	.extern _bss_top, _bss_end, ccp, trapinit, psa
	.extern _sysseg, _usrseg, _usrdseg, _sysstk

_start:
entry:
	di	vi, nvi
	ld	r2, #_bss_top
	ld	r4, #_bss_end
	ld	r5, r4
	sub	r5, r2
	test	r5
	jr	z, 2f
1:
	clrb	@r2
	inc	r2, #1
	djnz	r5, 1b
2:
	ld	r15, #0xFF00
	ld	_sysstk, r15
	clr	_sysseg
	ld	_usrseg, #0x0100
	ld	_usrdseg, #0x0100
	lda	r2, psa
	ldctl	psap, r2
	call	trapinit
	jp	ccp

_wboot:
	ld	r15, _sysstk
	jp	ccp
