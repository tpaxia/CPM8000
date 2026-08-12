! Z8002 trap bridge for the hosted emulator.  CP/M and host services share
! the file/disk implementation, but this frame and memory ABI are Z8002-only.

	.include "biosdef.s"

	.extern __bdos
	.extern fp_epu
	.global trapinit, _trap, _trap_ret, bdossc, biossc, memsc
	.global _bios, _bdos, _xfer, _mem_cpy, _map_adr
	.global _sysseg, _usrseg, _usrdseg, _sysstk, psa, _trapvec

	sect .text
	unsegm

sc_trap:
	push	@r15, @r15
_trap:
	sub	r15, #FRAMESZ-8
	ldm	@r15, r0, #15
	ldctl	r1, nsp
	ex	r1, nr15(r15)
	cpb	rh1, #0x7F
	jr	ne, trap_disp
	clrb	rh1
	add	r1, #SC0TRAP
trap_disp:
	sll	r1, #2
	ldl	rr0, _trapvec(r1)
	testl	rr0
	jr	z, _trap_ret
	call	@r1
_trap_ret:
	ld	r1, nr15(r15)
	ldctl	nsp, r1
	ldm	r0, @r15, #15
	add	r15, #FRAMESZ-6
	iret

epu_trap:
	push	@r15, #EPUTRAP
	jr	_trap
pi_trap:
	push	@r15, #PITRAP
	jr	_trap
seg_trap:
	push	@r15, #SEGTRAP
	jr	_trap
nmi_trap:
	push	@r15, #NMITRAP
	jr	_trap

bdossc:
	ld	r4, scfcw+2(r15)
	out	#PORT_BDOS, r0
	test	r0
	jr	z, bdos_native
	ld	cr6+2(r15), r6
	ld	cr7+2(r15), r7
	ret
bdos_native:
	pushl	@r15, rr6
	push	@r15, r7
	push	@r15, r5
	call	__bdos
	add	r15, #8
	ld	cr6+2(r15), r6
	ld	cr7+2(r15), r7
	ret

biossc:
	ld	r2, scfcw+2(r15)
	out	#PORT_BIOS, r0
	cp	r3, #1
	jp	eq, _wboot
	ld	cr6+2(r15), r6
	ld	cr7+2(r15), r7
	ret

memsc:
	cp	r5, #-2
	jr	eq, xfersc
	testl	rr2
	jr	nz, memsc_cpy
	ld	r4, scfcw+2(r15)
	out	#PORT_MAP, r0
	cp	r5, #-1
	jr	ne, 1f
	ld	_usrseg, r6
	ld	_usrdseg, r6
1:
	cp	r5, #4
	jr	ne, 2f
	ld	_usrdseg, r6
2:
	ld	cr6+2(r15), r6
	ld	cr7+2(r15), r7
	ret
memsc_cpy:
	out	#PORT_MEMCPY, r0
	ld	cr6+2(r15), r6
	ld	cr7+2(r15), r7
	ret

xfersc:
	inc	r15, #2
	ld	r5, r15
	ld	r2, #FRAMESZ/2
	ldir	@r5, @r7, r2
	dec	r5, #2
	ldi	@r5, @r7, r2
	jr	_trap_ret

_bios:
	ld	r3, ARG1(r15)
	ldl	rr4, ARG2(r15)
	ldl	rr6, ARG4(r15)
	sc	#BIOS_SC
	ret
_bdos:
	ld	r5, ARG1(r15)
	ldl	rr6, ARG2(r15)
	sc	#BDOS_SC
	ret
_xfer:
	ldl	rr6, ARG1(r15)
	ldl	rr4, #-2
	subl	rr2, rr2
	sc	#XFER_SC
	ret
_mem_cpy:
	ldl	rr6, ARG1(r15)
	ldl	rr4, ARG3(r15)
	ldl	rr2, ARG5(r15)
	sc	#MEM_SC
	ret
_map_adr:
	ldl	rr6, ARG1(r15)
	ld	r5, ARG3(r15)
	subl	rr2, rr2
	sc	#MEM_SC
	ret

trapinit:
	lda	r2, _trapvec
	ld	r0, #NTRAPS
	subl	rr4, rr4
1:
	ldl	@r2, rr4
	inc	r2, #4
	djnz	r0, 1b
	subl	rr2, rr2
	lda	r3, bdossc
	ldl	_trapvec+(BDOS_SC+SC0TRAP)*4, rr2
	lda	r3, biossc
	ldl	_trapvec+(BIOS_SC+SC0TRAP)*4, rr2
	lda	r3, memsc
	ldl	_trapvec+(MEM_SC+SC0TRAP)*4, rr2
	lda	r3, fp_epu
	ldl	_trapvec+EPUTRAP*4, rr2

	lda	r5, psa
	ld	r0, #0x5800
	add	r5, #4
	ld	r1, #epu_trap
	ldm	@r5, r0, #2
	add	r5, #4
	ld	r1, #pi_trap
	ldm	@r5, r0, #2
	add	r5, #4
	ld	r1, #sc_trap
	ldm	@r5, r0, #2
	add	r5, #4
	ld	r1, #seg_trap
	ldm	@r5, r0, #2
	add	r5, #4
	ld	r1, #nmi_trap
	ldm	@r5, r0, #2
	ret

	sect .psa
	.even
psa:
	.space 256

	sect .bss
	.even
_sysseg:	.space 2
_usrseg:	.space 2
_usrdseg:	.space 2
_sysstk:	.space 2
_trapvec:	.space NTRAPS*4
