/*
 * @(#)a.com.h	3.5
 *
 * Declarations common to all of the cross assemblers.
 */

/*
 * Version 3.5, 8/20/82.  Added extensions for the 8086.
 *
 * Version 3.4, 7/5/82.  Added scanc() and white() macros.
 *
 * Version 3.3, 5/27/82.  Added uext switch.
 *
 * Version 3.2, 5/22/82.  Added define for PDDIR.
 */

/*
 * Parameters.
 */
#define	BLKLOG	9		/* log base 2 of disk block size */
#define	IISIZ	30		/* size of parse stack in frames */
#define	INSIZ	4096		/* size of input stack in bytes */
#define	LLERR	3		/* length of error field in listing line */
#define	LLLOC	4		/* length of location field in listing line */
#define	LLOBJ	8		/* length of object field in listing line */
#define	LLPP	58		/* listing lines/page (must be less than 64) */
#define	LLSEQ	5		/* length of sequence field in listing line */
#define	OBJSIZ	255		/* maximum object block length */
#define	OHSHLOG	6		/* log base 2 of opcode hash table size */

#ifndef PDDIR			/* following define overridable in cc line */
#define	PDDIR	"/usr/lib"	/* directory containing the predef file */
#endif	PDDIR

#define	SECSIZ	255		/* maximum number of sections including abs */
#define	SHSHLOG	9		/* log base 2 of symbol hash table size */
#define	SLINSIZ	128		/* maximum source line length */
#define	STRSIZ	128		/* maximum string length */
#define	SYMSIZ	8		/* maximum symbol length */
#define	TITSIZ	47		/* maximum title string length */
/*
 * Assembler directive numbers.
 */
#define	ADABS	19		/* .abs */
#define	ADALIGN	20		/* .align */
#define	ADBLOCK	7		/* .block */
#define	ADBYTE	4		/* .byte */
#define	ADCLIST	9		/* .clist */
#define	ADCOM	26		/* .common */
#define	ADEJECT	25		/* .eject */
#define	ADELSE	15		/* .else */
#define	ADEND	1		/* .end */
#define	ADENDIF	16		/* .endif */
#define	ADENDM	23		/* .endm */
#define	ADENDR	28		/* .endr */
#define	ADEQU	2		/* .equ */
#define	ADERROR	30		/* .error */
#define	ADEXIT	31		/* .exit */
#define	ADGLOB	18		/* .global */
#define	ADIF	17		/* .if */
#define	ADINPUT	3		/* .input */
#define	ADLIST	14		/* .list */
#define	ADLONG	24		/* .long */
#define	ADMAC	22		/* .macro */
#define	ADORG	8		/* .org */
#define	ADREPT	29		/* .repeat */
#define	ADSECT	10		/* .sect */
#define	ADSET	6		/* .set */
#define	ADSPACE	11		/* .space */
#define	ADSTITL	12		/* .stitle */
#define	ADTITLE	13		/* .title */
#define	ADWARN	33		/* .warn */
#define	ADWITHN	21		/* .within */
#define	ADWORD	5		/* .word */
/*
 * Parsing constants and flags.
 */
#define	IIAFLG	020000		/* alternate left parts flag */
#define	IIDSYM	0377		/* default symbol in scntab */
#define	IIESYM	0376		/* error symbol in scntab */
#define	IILMSK	0007		/* production length mask */
#define	IINFLG	0400		/* nonterminal symbol flag */
#define	IIRFLG	040000		/* read and reduce flag */
#define	IIXFLG	0100000		/* transition flag */
/*
 * Input stack frame types (in in_typ).
 */
#define	INFILE	1		/* file */
#define	INMAC	2		/* macro expansion */
#define	INRPT	3		/* repeat */
#define	INARG	4		/* macro argument */
/*
 * Operand classes.
 */
#define	OCNULL	0x00		/* no operand */
#define	OCNEX	0x01		/* nn */
#define	OCPEX	0x02		/* (nn) */
#define	OCEXP	0x03		/* {nn,(nn)} */
/*
 * Operand flags (in op_flg and ps_flg).
 */
#define	OFFOR	0x8000		/* operand contains a forward reference */
/*
 * Opcode types (in oc_typ).
 */
#define	OTUND	0		/* undefined */
#define	OTINS	1		/* instruction */
#define	OTDIR	2		/* directive */
#define	OTMAC	3		/* macro */
/*
 * Symbol attribute flags (in sy_atr).
 */
#define	SADP2	0001		/* defined in pass 2 */
#define	SAMUD	0002		/* multiply defined */
#define	SAGLO	0004		/* global */
/*
 * Symbol types (in sy_typ).
 */
#define	STUND	0		/* undefined */
#define	STKEY	1		/* keyword */
#define	STSEC	2		/* section */
#define	STLAB	3		/* label or equ */
#define	STVAR	4		/* variable (redefinable) */
/*
 * Token types and values returned from the lexical scanner.
 */
#define	TKEOF	0		/* end of file -- must be 0 */
#define	TKDOL	1		/* dollar sign */
#define	TKSYM	2		/* symbol */
#define	TKCON	3		/* constant */
#define	TKSTR	4		/* quoted string */
#define	TKUNOP	5		/* unary operator */
#define	TKMULOP	6		/* multiplicative operator */
#define		TVMUL	1		/* multiplication */
#define		TVDIV	2		/* division */
#define		TVMOD	3		/* modulo */
#define		TVSHL	4		/* shift left */
#define		TVSHR	5		/* shift right */
#define	TKADDOP	7		/* additive operator */
#define		TVADD	1		/* addition */
#define		TVSUB	2		/* subtraction */
#define	TKRELOP	8		/* relational operator */
#define		TVEQ	1		/* equal */
#define		TVNE	2		/* not equal */
#define		TVLT	3		/* less than */
#define		TVGT	4		/* greater than */
#define		TVLE	5		/* less than or equal */
#define		TVGE	6		/* greater than or equal */
#define	TKANDOP	9		/* logical and operator */
#define	TKXOROP	10		/* exclusive or operator */
#define	TKOROP	11		/* inclusive or operator */
#define	TKLPAR	12		/* left parenthesis */
#define	TKRPAR	13		/* right parenthesis */
#define	TKCOM	100		/* comma */
#define	TKCOLON	101		/* colon */
#define	TKEOL	102		/* end of line */
#define	TKSPC	103		/* white space */
#define	TKERR	127		/* erroneous token */
/*
 * Virtual memory buffer flags (in vm_flg).
 */
#define	VMDIR	01		/* dirty bit */
/*
 * Cross reference masks (in xr_pl).
 */
#define	XRDEF	0100000		/* Symbol defined at this reference */
/*
 * Type definitions.
 */
#define	exprval	long		/* expression value */
#define	reg	register	/* abbreviation for good tabbing */
#define	uns	unsigned	/* abbreviation for good tabbing */
#define	vmadr	unsigned	/* virtual memory address */
/*
 * Pseudo-variables.
 */
#define	ERROR	stdout		/* ERRORs go to standard output */
FILE	*LIST;			/* pointer to listing FILE structure */
FILE	*OBJECT;		/* pointer to object FILE structure */
/*
 * Pseudo-functions.
 */
#define scanc()		((ch = *scanpt++)!='\0'&&ch!=escchr ? ch : xscanc())
#define	unscanc()	(ch = *--scanpt)
#define white(c)	(c==' ' || c=='\t')
/*
 * Structure declarations.
 */
struct	aside {			/* symbol lookaside table entry */
	struct	aside	*as_lnk;	/* lru chain link */
	vmadr		as_sym;		/* symbol table entry pointer */
	char		as_str[SYMSIZ];	/* symbol */
};
struct	chent {			/* single-character token table entry */
	char	ch_chr;			/* character */
	char	ch_typ;			/* token type */
	char	ch_val;			/* token value */
};
struct	input {			/* input stack frame */
	struct	input	*in_ofp;	/* pointer to previous frame */
	char	in_typ;			/* frame type */
	char	in_rpt;			/* repeat count */
	char	in_lst;			/* listing level */
	char	*in_ptr;		/* pointer to next character */
	int	in_cnt;			/* number of characters left */
	int	in_fd;			/* file descriptor */
	uns	in_seq;			/* line number */
	char	in_buf[1<<BLKLOG];	/* buffer area (variable size & use */
};
struct	octab {			/* opcode table entry */
	struct	octab	*oc_lnk;	/* link to next entry in hash chain */
	uns	oc_val;			/* value of opcode */
	char	oc_typ;			/* type of opcode */
	char	oc_arg;			/* highest formal number for macro */
	char	oc_str[SYMSIZ];		/* opcode mnemonic string */
};
struct	operand {		/* operand descriptor */
	long	op_cls;			/* set of classes (bit vector) */
	exprval	op_val;			/* value of operand */
	uns	op_rel;			/* relocation of operand */
	int	op_flg;			/* flags */
};
struct	output {		/* output buffering structure */
	char	*ou_ptr;		/* pointer to next character */
	int	ou_cnt;			/* number of characters left */
	int	ou_fd;			/* file descriptor */
	char	ou_buf[1<<BLKLOG];	/* buffer */
};
struct	psframe {		/* parse stack frame */
	int	*ps_state;		/* parse state */
	int	ps_sym;			/* lookahead symbol */
	exprval	ps_val0;		/* usually subexpression value */
	uns	ps_val1;		/* usually subexpression relocation */
	int	ps_flg;			/* flags */
};
struct	section {		/* section table entry */
	vmadr	se_sym;			/* symbol table pointer */
	uns	se_loc;			/* location counter */
	char	se_aln;			/* alignment */
	char	se_ext;			/* extent */
	char	se_atr;			/* attributes */
};
struct	sytab {			/* symbol table entry */
	vmadr	sy_lnk;			/* link to next entry in hash chain */
	vmadr	sy_xlk;			/* link to rear of xref chain */
	exprval	sy_val;			/* value of symbol */
	uns	sy_rel;			/* relocation of symbol */
	char	sy_typ;			/* type of symbol */
	char	sy_atr;			/* attributes of symbol */
	char	sy_str[SYMSIZ];		/* symbol mnemonic string */
};
struct	vmbuf {			/* virtual memory buffer */
	struct	vmbuf	*vm_lnk;	/* lru link to next buffer */
	short	vm_blk;			/* disk block number */
	short	vm_flg;			/* flags */
	char	vm_mem[1<<BLKLOG];	/* disk block data storage */
};
struct	xref {			/* cross reference entry */
	vmadr	xr_lnk;			/* circular link to next entry */
	int	xr_pl;			/* page and line number */
};
/*
 * Global variable declarations.
 */
char		argchr;		/* macro argument designator character */
struct	aside	*ashead;	/* symbol lookaside lru chain head */
struct	aside	aspool[];	/* pool of symbol lookaside entries */
int		ch;		/* current character from scanc */
struct	chent	chtab[];	/* single-character token table */
uns		condlev;	/* nesting level of conditionals */
char		condlst;	/* flag enabling listing of skipped code */
uns		curaln;		/* alignment for current section */
uns		curatr;		/* attributes for current section */
struct	octab	*curdef;	/* current macro being defined */
uns		curext;		/* extent for current section */
exprval		curloc;		/* current location counter value */
uns		curlst;		/* current listing level */
struct	operand	curop;		/* current operand information */
uns		cursec;		/* current section number */
int		curxpl;		/* current xref page and line */
char		datstr[];	/* string containing date and time */
uns		deflev;		/* macro definition nesting level */
char		eflg;		/* expression error flag */
extern	char	oflag;		/* Flag to Make Labled output file */
extern	char	segflg;		/* Flag to Make Segmented output file */
uns		errct;		/* error count */
char		escchr;		/* escape character */
int		extoff;		/* offset of source extension in program name */
struct	psframe	iilexeme;	/* info returned from lexical scanner */
int		iilset;		/* alternate left parts set */
int		iilsym;		/* new left part symbol */
struct	psframe	iips[];		/* parsing stack */
struct	psframe	*iipsp;		/* parsing stack pointer */
struct	psframe	*iipspl;	/* parsing left end pointer */
struct	input	*infp;		/* input frame pointer */
char		*insp;		/* input stack pointer */
int		instk[];	/* input stack */
vmadr		label;		/* sytab pointer for statement label */
char		labstr[];	/* label string */
char		lbrchr;		/* left brace character for macro args */
char		lflag;		/* flag set if listing being generated */
uns		linect;		/* number of lines left on listing page */
char		llerr[];	/* error field of listing line */
char		*llert;		/* top of error field */
char		llfull;		/* flag indicating something to list */
char		llloc[];	/* location field in listing line */
char		llobj[];	/* object field in listing line */
char		*llobt;		/* top of object field */
char		llseq[];	/* sequence field in listing line */
char		llsrc[];	/* source field in listing line */
char		mctchr;		/* macro expansion count character */
uns		mexct;		/* count of macro expansions */
uns		minaln;		/* minimum section alignment value */
int		ntdflt[];	/* nonterminal default action table */
exprval		nxtloc;		/* next location for text output */
uns		nxtsec;		/* next section for text output */
char		objbuf[];	/* object block construction area */
char		*objtop;	/* top of text info in objbuf */
char		objtyp;		/* object block type being built */
struct	octab	*ochtab[];	/* opcode hash table */
struct	octab	*opcode;	/* octab pointer for statement opcode */
char		opcstr[];	/* opcode string */
struct	operand	optab[];	/* instruction operands description */
uns		pagect;		/* listing page number */
char		parsing;	/* flag indicating we are parsing */
char		pass2;		/* flag indicating we are in pass 2 */
char		pflag;		/* flag indicating local predef file */
char		*phylim;	/* first unallocated memory location */
char		*phytop;	/* first unused memory location */
int		prevsem;	/* most recent semantic routine number */
char		*prname;	/* name of this assembler */
int		ptab[];		/* parsing action table */
char		rbrchr;		/* right brace character for macro args */
char		reading;	/* flag indicating we are reading input */
char		*relbot;	/* bottom of relocation info in objbuf */
uns		rmarg;		/* listing right margin column */
char		rptct;		/* repeat count for current repeat def */
uns		rptlev;		/* repeat definition nesting level */
vmadr		rptstr;		/* start of repeat definition in vm */
int		savlen;		/* length of string in savstr */
char		savstr[];	/* first string of string comparison */
char		*scanpt;	/* pointer to next character in sline */
char		scntab[];	/* symbol scanning table */
uns		secct;		/* number of sections defined */
struct	section	sectab[];	/* section table */
int		semtab[];	/* semantic action table */
char		sline[];	/* buffer holding current source line */
char		*srcfile;	/* source file name pointer */
vmadr		syhtab[];	/* symbol hash table */
char		titl1[];	/* first title line */
char		titl2[];	/* second title line */
char		tokstr[];	/* string from token scanner */
int		toktyp;		/* token type from token scanner */
exprval		tokval;		/* value from token scanner */
uns		truelev;	/* true conditional nesting level */
char		uext;		/* flag turning undef. syms into externals */
char		uflg;		/* undefined symbol in expression flag */
vmadr		virtop;		/* first unused vm location */
int		vmfd;		/* virtual memory file descriptor */
struct	vmbuf	*vmhead;	/* head of vm lru chain */
struct	vmbuf	vmpool[];	/* pool of virtual memory buffers */
uns		warnct;		/* warning count */
char		xflag;		/* flag enabling cross referencing */

#ifdef	STATS
uns		ashct;		/* number of symbol lookaside hits */
uns		chnct;		/* total sytab links followed */
char		stats;		/* flag enabling statistics */
uns		sylct;		/* number of calls to sylook */
uns		symct;		/* number of symbols */
uns		vmgct;		/* number of vm accesses */
uns		vmrct;		/* number of vm disk reads */
uns		vmwct;		/* number of vm disk writes */
#endif
/*
 * Function declarations.
 */
struct	sytab	*assign();
uns		hash();
struct	octab	*oclook();
struct	input	*pushin();
char		*rfetch();
char		*rindex();
vmadr		sylook();
vmadr		symerge();
vmadr		valloc();
char		*wfetch();
