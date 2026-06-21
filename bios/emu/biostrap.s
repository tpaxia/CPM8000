! biostrap.s - Trap handler for CP/M-8000 emulator BIOS
! SC calls dispatch through the Z8001 native trap mechanism:
!   SC instruction → hardware trap → PSA dispatch → _trap → handler
! BDOS/BIOS handlers bridge to C++ via I/O port OUT instructions.
! xfer copies context to the trap frame and returns via IRET.

	.include "biosdef.s"

	.extern	_sysseg, _psap
	.extern	__bdos

	.global	trapinit, _trap, _trap_ret, psa
	.global	xfersc, memsc, bdossc, biossc

	unsegm
	sect .text

! System call trap handler (entered in segmented mode)
sc_trap:
	push	@r14, @r14
_trap:
	sub	r15, #30
	ldm	@r14, r0, #14
	NONSEG
	ldctl	r1, nsp
	ld	nr14(r15), r14
	ex	r1, nr15(r15)

	! trap# in r1
	cpb	rh1, #0x7F
	jr	ne, trap_disp
	clrb	rh1
	add	r1, #SC0TRAP

trap_disp:
	sll	r1, #2
	ldl	rr0, _trapvec(r1)
	testl	rr0
	jr	z, _trap_ret

	pushl	@r15, rr0
	SEG
	popl	rr0, @r14
	calr	trap_1
	jr	_trap_ret
trap_1:
	pushl	@r14, rr0
	ret

_trap_ret:
	NONSEG
	ld	r1, nr15(r15)
	ld	r14, nr14(r15)
	ldctl	nsp, r1
	SEG
	ldm	r0, @r14, #14
	add	r15, #32
	iret

! Assorted trap handlers
epu_trap:
	push	@r14, #EPUTRAP
	jr	_trap

pi_trap:
	push	@r14, #PITRAP
	jr	_trap

seg_trap:
	push	@r14, #SEGTRAP
	jr	_trap

nmi_trap:
	push	@r14, #NMITRAP
	jr	_trap

!----------------------------------------------------------------------
! BDOS system call handler (SC #2)
! Entered in SEG mode from trap dispatcher via calr (4-byte ret addr).
! Frame is at r15+4. Caller's r5/r6/r7 still in CPU registers.
! r5 = BDOS function number
! rr6 = parameter (r6=seg, r7=off)
!
! The C++ BDOS router (PORT_BDOS) decides per-call which backend serves
! the request:
!   r0 = 1  -> handled in C++ (HOST_DIR drive); result already in rr6.
!   r0 = 0  -> defer to the native BDOS (IMAGE drives + console/system).
! Native disk I/O then goes through BIOS block I/O (SC #3) to disk images.
bdossc:
	NONSEG
	ld	r4, scseg+4(r15)	! caller's PC segment from frame
	! Adjust segment for non-seg callers (matches M20 biossc pattern)
	ld	r0, scfcw+4(r15)
	and	r0, #0xC000
	jr	nz, 1f
	ld	r6, r4			! non-seg: add caller seg to param
1:
	out	#PORT_BDOS, r0		! C++ router: sets r0 = handled flag (and rr6 if handled)
	test	r0
	jr	z, bdos_native		! r0==0 -> native BDOS
	! Handled by C++ (HOST_DIR drive): result is in rr6 (r6:r7)
	ld	cr6+4(r15), r6
	ld	cr7+4(r15), r7
	SEG
	ret

bdos_native:
	! Call real BDOS internal dispatcher: long __bdos(int func, long param)
	! Uses __bdos (internal C dispatcher with switch table) instead of _bdos
	! (SC #2 thunk) to avoid infinite recursion through our smart bdossc.
	pushl	@r15, rr6		! push param (long)
	push	@r15, r5		! push func
	call	__bdos
	add	r15, #6			! clean stack
	ld	cr6+4(r15), r6		! store return rr6
	ld	cr7+4(r15), r7
	SEG
	ret

!----------------------------------------------------------------------
! BIOS system call handler (SC #3)
! Entered in SEG mode from trap dispatcher via calr (4-byte ret addr).
! Frame is at r15+4. Caller's r3-r7 still in CPU registers.
! r3 = BIOS function number
! rr4 = P1, rr6 = P2
biossc:
	NONSEG
	ld	r2, scseg+4(r15)	! caller's PC segment from frame
	out	#PORT_BIOS, r0		! trigger C++ BIOS handler (reads r2-r7)
	! C++ handler sets r6/r7 with return values
	ld	cr6+4(r15), r6		! store return r6 to frame
	ld	cr7+4(r15), r7		! store return r7 to frame
	SEG
	ret

!----------------------------------------------------------------------
! Memory manager (SC #0 and SC #1)
! Dispatches based on caller's r5 value:
!   0xFFFE = xfer (context transfer)
!   0xFFFF = set_user_seg (no-op)
!   rr2 != 0 = mem_cpy
!   else = map_adr
memsc:
	cp	r5, #0xFFFE
	jr	eq, xfersc
	cp	r5, #0xFFFF
	jr	eq, memsc_ret		! set_user_seg: no-op
	! Check length (rr2) for mem_cpy vs map_adr
	testl	rr2
	jr	nz, memsc_cpy
	! map_adr: rr6=addr, r5=space code
	NONSEG
	ld	r4, scseg+4(r15)	! caller's PC segment
	out	#PORT_MAP, r0		! trigger C++ map_adr handler
	ld	cr6+4(r15), r6		! store mapped segment to frame
	ld	cr7+4(r15), r7
	SEG
	ret
memsc_cpy:
	! mem_cpy: rr2=length, rr4=dest, rr6=source
	NONSEG
	out	#PORT_MEMCPY, r0	! trigger C++ mem_cpy handler
	ld	cr6+4(r15), r6		! store result to frame
	ld	cr7+4(r15), r7
	SEG
	ret
memsc_ret:
	ret

!----------------------------------------------------------------------
! Context transfer (xfer)
! Called when r5 == 0xFFFE in memsc.
! RR6 = address of context structure (40 bytes).
! Copy context block onto the trap frame, then return via _trap_ret
! so IRET pops the new FCW and full segmented PC.
xfersc:
	SEG
	inc	r15, #4			! skip calr return address
	ldl	rr4, rr14		! rr4 = frame pointer (system stack)
	ld	r2, #FRAMESZ / 2	! word count
	ldir	@r4, @r6, r2		! block copy context to frame
	jp	_trap_ret

!----------------------------------------------------------------------
! System call thunks for C code (BDOS/CCP) to invoke BIOS services.
! These override the versions in libcpm.a to ensure correct calling
! convention: _bios(int code, long p1, long p2).

	.global	_bios, _bdos, _xfer, _mem_cpy, _map_adr

! long _bios(int code, long p1, long p2)
_bios:
	ld	r3, ARG1(r15)		! code (int)
	ldl	rr4, ARG2(r15)		! p1 (long)
	ldl	rr6, ARG4(r15)		! p2 (long)
	sc	#BIOS_SC
	ret

! long _bdos(int code, long param)
_bdos:
	ld	r5, ARG1(r15)		! code (int)
	ldl	rr6, ARG2(r15)		! param (long)
	sc	#BDOS_SC
	ret

! _xfer(long context) - context switch, never returns
_xfer:
	ldl	rr6, ARG1(r15)		! context address (long)
	ldl	rr4, #-2		! r4=0xFFFF, r5=0xFFFE (xfer magic)
	subl	rr2, rr2
	sc	#XFER_SC
	ret

! _mem_cpy(long source, long dest, long length)
_mem_cpy:
	ldl	rr6, ARG1(r15)		! source (long)
	ldl	rr4, ARG3(r15)		! dest (long)
	ldl	rr2, ARG5(r15)		! length (long)
	sc	#MEM_SC
	ret

! long _map_adr(long addr, int space)
_map_adr:
	ldl	rr6, ARG1(r15)		! addr (long)
	ld	r5, ARG3(r15)		! space (int)
	subl	rr2, rr2		! 0 length = map_adr mode
	sc	#MEM_SC
	ret

! FPE handler stub
	.global fp_epu
fp_epu:
	ret

! PSA entry sizes
	.equ	ps, 8
	.equ	psa_epu, 1 * ps
	.equ	psa_prv, 2 * ps
	.equ	psa_sc,  3 * ps
	.equ	psa_seg, 4 * ps
	.equ	psa_nmi, 5 * ps

! Trap initialization
trapinit:
	lda	r2, _trapvec
	ld	r0, #NTRAPS
	subl	rr4, rr4
clrtraps:
	ldl	@r2, rr4
	inc	r2, #4
	djnz	r0, clrtraps

	! Set up trap vectors (bdossc installed by biosboot after _bdosini)
	ld	r2, _sysseg
	lda	r3, biossc
	ldl	_trapvec + (BIOS_SC + SC0TRAP) * 4, rr2
	lda	r3, memsc
	ldl	_trapvec + (MEM_SC + SC0TRAP) * 4, rr2
	! SC #0 also dispatches to memsc (xfer comes through SC #0 or SC #1)
	ldl	_trapvec + (0 + SC0TRAP) * 4, rr2
	lda	r3, fp_epu
	ldl	_trapvec + EPUTRAP * 4, rr2

	! Initialize PSA entries
	ldl	rr4, _psap
	SEG
	ldl	rr0, #0x0000D800	! FCW: system mode, ints enabled

	add	r5, #ps			! EPU trap
	ldar	r2, epu_trap
	ldm	@r4, r0, #4

	add	r5, #ps			! Privileged Inst
	ldar	r2, pi_trap
	ldm	@r4, r0, #4

	add	r5, #ps			! System Call
	ldar	r2, sc_trap
	ldm	@r4, r0, #4

	add	r5, #ps			! Segmentation
	ldar	r2, seg_trap
	ldm	@r4, r0, #4

	add	r5, #ps			! Non-Maskable Int
	ldar	r2, nmi_trap
	ldm	@r4, r0, #4

	NONSEG
	ret

!----------------------------------------------------------------------
	sect .psa
psa:
	.space 100
