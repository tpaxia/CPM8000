!------------------------------------------------------------------------------
! i8251.s
!  Serial I/O routines for SCC
!
!  Copyright (c) 2022 tpaxia
!------------------------------------------------------------------------------

	 .global  scc_init, scc_out, scc_in, scc_status
	
	sect	.text
	unsegm


	.equ	c8253ctrl, 0x0127   ! control register 
	.equ	c8253c0, 0x0121		! counter 0 (TTY/printer timing)
	.equ    c8253c1, 0x0123		! counter 1 (keyboard timing)

	.equ	TTYC, 0xC3	
	.equ	TTYD, 0xC1

!------------------------------------------------------------------------------
! initscc
!   Initialize i8353 Timer for 19200 baud rate
!
!   destroyed:  r2, r3, r4 

scc_init:
	! set up TTY divisor

	ldb  rl2, #0x36
	outb #c8253ctrl,rl2
	ld  r2, #0x0080
	outb #c8253c0,rl2
	outb #c8253c0,rh2

	! set up Keyboard divisor

	ldb  rl2, #0x76
	outb #c8253ctrl,rl2
	ld  r2, #0x0080
	outb #c8253c1,rl2
	outb #c8253c1,rh2

	ld      r2, #(scccmde2 - scccmds2)    ! initialize 8251
	ld      r3, #TTYC
	ld      r4, #scccmds2
	otirb   @r3, @r4, r2
	ret
	
	 

!------------------------------------------------------------------------------
! scc_out 
!   output 1 byte to the serial port
!   input:      r5 --- Ascii code
!   destroyed:  rl0

scc_out:
	inb	rl0, #TTYC
	andb	rl0, #0x01
	jr	z, scc_out
	outb	#TTYD, rl5
	ret

!------------------------------------------------------------------------------
! scc_in
!   input 1 byte from the serial port
!   return:     r7 --- read data
!   destroyed:  r0, r1

scc_in:
	inb	rl0, #TTYC
	andb	rl0, #0x02
	jr	z, scc_in
	clr     r7
	inb	rl7, #TTYD
	ret
!------------------------------------------------------------------------------
! scc_status
!   return:     r7 --- 0xff:data exists, 0x00:no data
!   destroyed:  r1, r10

scc_status:
    inb     rl0, #TTYC
    clr     r7
    andb    rl0, #0x02
    ret     z
    com     r7
    ret


!------------------------------------------------------------------------------
	sect .rodata

scccmds2:
	.byte	0x0		! Sync mode 
	.byte	0x0		! Sync char1 
	.byte	0x0		! Sync char2
	.byte	0x40	! Reset device
	.byte	0x4d	! ASync mode, 1x, N, 8, 1
	.byte	0x37	! Err reset, enable TX & RX
	.byte	0x27	! Err reset, enable TX & RX
scccmde2:
