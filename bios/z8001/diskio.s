!------------------------------------------------------------------------------
! diskio.s
!   Disk I/O subroutines 
!
!   Copyright(c) 2020 4sun5bu
!
!
	.global	diskrd, diskwr, disk_init
	
	.equ	IDE_DATAW, 0x01c0	! Word access
	.equ	IDE_DATAB, 0x01c1	! Byte access
	.equ	IDE_ERROR, 0x01c3
	.equ	IDE_FEATURES, 0x01c3
	.equ	IDE_SECT_CNT, 0x01c5
	.equ	IDE_SECT_NUM, 0x01c7
	.equ	IDE_CYL_LOW, 0x01c9
	.equ	IDE_CYL_HIGH, 0x01cb
	.equ	IDE_DEV_HEAD, 0x01cd
	.equ	IDE_STATUS, 0x01cf
	.equ	IDE_COMND, 0x01cf


	.equ	BSY_BIT, 7
	.equ	DRDY_BIT, 6
	.equ	DRQ_BIT, 3
	.equ	ERR_BIT, 0

	.macro	CHKBSY
bsy_loop\@:
	inb	rl0, #IDE_STATUS
	bitb	rl0, #BSY_BIT
	jr	nz, bsy_loop\@ 
	.endm

	.macro	CHKDRDY
drdy_loop\@:
	inb	rl0, #IDE_STATUS
	bitb	rl0, #DRDY_BIT
	jr	z, drdy_loop\@
	.endm  

	.macro	CHKDRQ
drq_loop\@:
	inb	rl0, #IDE_STATUS
	bitb	rl0, #DRQ_BIT
	jr	z, drq_loop\@
	.endm

	unsegm
	sect	.text
	.macro	CMDRDY

cmdrdy_loop\@:
	inb	rl0, #IDE_STATUS
	bitb	rl0, #BSY_BIT
	jr	nz, cmdrdy_loop\@
	bitb	rl0, #DRDY_BIT
	jr	z, cmdrdy_loop\@
	.endm


	.macro	DATRDY
dat_loop\@:
	inb	rl0, #IDE_STATUS
	bitb	rl0, #BSY_BIT
	jr	nz, dat_loop\@
	bitb	rl0, #DRQ_BIT
	jr	z, dat_loop\@
	.endm
!------------------------------------------------------------------------------
disk_init:
	CHKBSY
	CHKDRDY
	
	ldb	rl0, #0x01		! Byte access 
	outb	#IDE_FEATURES, rl0
	ldb	rl0, #0xef
	outb	#IDE_COMND, rl0
	CHKBSY

	ldb	rl0, #0x82
	outb	#IDE_FEATURES, rl0
	ldb	rl0, #0xef
	outb	#IDE_COMND, rl0
	CHKBSY
	ret 
	
!------------------------------------------------------------------------------
!  diskrd
!    One sector read
!    input  rr2 --- LBA
!	    r4  --- Buffer Address

diskrd:
	CMDRDY
	CHKBSY
	
	ldb	rl0, #1
	outb	#IDE_SECT_CNT, rl0
	outb	#IDE_SECT_NUM, rl3
	outb	#IDE_CYL_LOW, rh3
	outb	#IDE_CYL_HIGH, rl2
	andb	rh2, #0x0f
	orb	rh2, #0x40
	outb	#IDE_DEV_HEAD, rh2
	ldb	rl0, #0x20		! data in command
	outb	#IDE_COMND, rl0
	DATRDY

	inb	rl0, #IDE_STATUS	! Reset INTRQ
	ld	r2, #512
1:
	inb	rl0, #IDE_DATAB
	ldb	@r4, rl0
	inc	r4, #1
	djnz    r2,1b

	inb	rl0, #IDE_STATUS    ! 
	ret
	
!------------------------------------------------------------------------------
!  diskwr
!    One sector write
!    input rr2 --- LBA
!	    r4 --- Buffer Address

diskwr:
	CHKBSY
	CHKDRDY

	ldb	rl0, #1
	outb	#IDE_SECT_CNT, rl0
	xorb	rl0, rl0
	outb	#IDE_SECT_NUM, rl3
	outb	#IDE_CYL_LOW, rh3
	outb	#IDE_CYL_HIGH, rl2
	andb	rh2, #0x0f
	orb	rh2, #0x40
	outb	#IDE_DEV_HEAD, rh2
	ldb	rl0, #0x30		! data out command
	outb	#IDE_COMND, rl0
	DATRDY

	inb	rl0, #IDE_STATUS	! Reset INTRQ
	ld	r2, #512
1:
	ldb	rl0, @r4
	outb	#IDE_DATAB, rl0
	inc	r4, #1
	djnz    r2,1b
	
	CHKBSY
	inb	rl0, #IDE_STATUS
	ret
