/*
 * @(#)asz8k.h	3.1
 *
 * Declarations specific to asz8k.
 */

/*
 * Parameters.
 */
#define	OPMAX	4		/* maximum number of instruction operands */
/*
 * Instruction format table flags (in fm_flg).
 */
#define	FMNIB7	0x000f		/* value to OR into nibble 7 */
#define	FMSKEL2	0x0010		/* generate 2nd skeleton word */
#define	FMNEGI	0x0020		/* negate immediate value for right shift */
#define	FMLAST	0x8000		/* final entry for this instruction */
/*
 * Operand actions (in fm_op[]).
 */
#define	OANULL	0x0000		/* operand not used */
#define	OANIB1	0x0100		/* pack into nibble 1 */
#define	OANIB2	0x0200		/* pack into nibble 2 */
#define	OANIB3	0x0300		/* pack into nibble 3 */
#define	OANIB5	0x0500		/* pack into nibble 5 */
#define	OANIB6	0x0600		/* pack into nibble 6 */
#define	OANIB7	0x0700		/* pack into nibble 7 */
#define	OABYT1	0x0800		/* pack into byte 1 */
#define	OAX2	0x0900		/* generate indexed addr using nibble 2 */
#define	OAX3	0x0a00		/* generate indexed addr using nibble 3 */
#define	OABA2	0x0b00		/* generate based address using nibble 2 */
#define	OABA3	0x0c00		/* generate based address using nibble 3 */
#define	OABX2	0x0d00		/* generate based-indexed addr using nibble 2 */
#define	OABX3	0x0e00		/* generate based-indexed addr using nibble 3 */
#define	OACFLAG	0x0f00		/* OR condition flag bit into nibble 2 */
#define	OANIB3S	0x1100		/* pack into low 3 bits of nibble 3 */
#define	OANIB3A	0x1200		/* AND into nibble 3 */
#define	OANIB3D	0x1300		/* decrement and pack into nibble 3 */
#define	OANIB7D	0x1700		/* decrement and pack into nibble 7 */
#define	OARA7	0x1800		/* generate 7-bit relative address */
#define	OARA8	0x1900		/* generate 8-bit relative address */
#define	OARA12	0x1a00		/* generate 12-bit relative address */
#define	OARA16	0x1b00		/* generate 16-bit relative address */
#define	OAIMM8	0x1c00		/* generate 8-bit immediate */
#define	OAIMM16	0x1d00		/* generate 16-bit immediate */
#define	OAIMM32	0x1e00		/* generate 32-bit immediate */
#define	OASHFT	0x1f00		/* generate shift count of 1 or 2 */
#define	OAMSK	0x1f00		/* mask for operand action field */
/*
 * Operand classes.
 */
#define	OCREG8	0x0004		/* {rh0,rh1,rh2,rh3,...,rl4,rl5,rl6,rl7} */
#define	OCREG16	0x0005		/* {r0,r1,r2,r3,...,r14,r15} */
#define	OCREG32	0x0006		/* {rr0,rr2,rr4,rr6,rr8,rr10,rr12,rr14} */
#define	OCREG64	0x0007		/* {rq0,rq4,rq8,rq12} */
#define	OCIMM	0x0008		/* immediate value */
#define	OCIR	0x0009		/* indirect register */
#define	OCX	0x000a		/* indexed address */
#define	OCBA	0x000b		/* based address */
#define	OCBX	0x000c		/* based-indexed address */
#define	OCCCODE	0x000d		/* condition code */
#define	OCCFLAG	0x000e		/* condition code flag bit */
#define	OCINT	0x000f		/* {nvi,vi} */
#define	OCCTL	0x0010		/* control register for LDCTL */
#define	OCFLAGS	0x0011		/* FLAGS for LDCTLB */
#define	OCIRIO	0x0012		/* indirect register for I/O instruction */
#define	OCREGA	0x0013		/* address register (depends on seg mode) */
#define	OCMSK	0x001f		/* mask for operand class field in fm_op */
/*
 * Operand flags (in op_flg and ps_flg).
 */
#define	OFXMSK	0x0f		/* mask for index register code */
#define	OFSHORT	0x80		/* force short offset */
/*
 * Token types.
 */
#define	TKPOUND	14		/* '#' */
#define	TKAT	15		/* '@' */
#define	TKREG8	16		/* {rh0,rh1,rh2,rh3,...rl4,rl5,rl6,rl7} */
#define	TKREG16	17		/* {r0,r1,r2,r3,...,r12,r13,r14,r15} */
#define	TKREG32	18		/* {rr0,rr2,rr4,rr6,rr8,rr10,rr12,rr14} */
#define	TKREG64	19		/* {rq0,rq4,rq8,rq12} */
#define	TKCCODE	20		/* condition codes and flags */
#define	TKINT	21		/* {nvi,vi} */
#define	TKCTL	22		/* control registers for LDCTL */
#define	TKFLAGS	23		/* FLAGS for LDCTLB */
/*
 * Structure declarations.
 */
struct	format {		/* instruction format table entry */
	int	fm_op[OPMAX];		/* operand descriptions */
	int	fm_skel;		/* opcode skeleton word */
	int	fm_flg;			/* flags */
};
/*
 * Global variable declarations.
 */
char		segflg;		/* Generate code for segmented processor */
