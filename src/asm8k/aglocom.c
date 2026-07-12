#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

#define NOBSS

/*
 * Version 3.3, 9/19/82.  Added conditional compilation on NOBSS to
 * avoid explicit initialization (and bulky load modules) where possible.
 *
 * Version 3.2, 5/27/82.  Added uext switch.
 */
static	char	ident[] = "@(#)a.glocom.c	3.3";

/*
 * Global variable definitions and initializations common to all of the
 * cross assemblers.
 */
char		argchr = '?';
struct	aside	*ashead = &aspool[0];
struct	aside	aspool[] = {
	{ &aspool[1] },
	{ &aspool[2] },
	{ &aspool[3] },
	{ &aspool[4] },
	{ &aspool[5] },
	{ &aspool[6] },
	{ &aspool[7] },
	{ &aspool[8] },
	{ &aspool[9] },
	{ &aspool[10] },
	{ &aspool[11] },
	{ &aspool[12] },
	{ &aspool[13] },
	{ &aspool[14] },
	{ &aspool[15] },
	{ &aspool[16] },
	{ &aspool[17] },
	{ &aspool[18] },
	{ &aspool[19] },
	{ &aspool[20] },
	{ &aspool[21] },
	{ &aspool[22] },
	{ &aspool[23] },
	{ &aspool[24] },
	{ &aspool[25] },
	{ &aspool[26] },
	{ &aspool[27] },
	{ &aspool[28] },
	{ &aspool[29] },
	{ 0 }
};
char		condlst = 1;
uns		curext = 32;
uns		curlst = 255;
struct	output	errout = { &errout.ou_buf[0], 0, 1 };
char		escchr = '\\';
struct	psframe	*iipsp = &iips[0];
struct	psframe	*iipspl = &iips[0];
char		*insp = instk;
char		lbrchr = '{';
char		*llert = &llerr[0];
char		*llobt = &llobj[0];
struct	output	lstout = { &lstout.ou_buf[0], 0, -1 };
char		mctchr = '#';
/*		ntdflt	is initialized in as*gram.c */
struct	output	objout = { &objout.ou_buf[0], 0, -1 };
char		*objtop = &objbuf[0];
char		*phylim;
char		*phytop;
/*		ptab	is initialized in as*gram.c */
char		rbrchr = '}';
char		reading = 1;
char		*relbot = &objbuf[OBJSIZ];
uns		rmarg = 80;
char		*scanpt = &sline[0];
/*		scntab	is initialized in as*gram.c */
uns		secct = 1;
/*		semtab	is initialized in as*gram.c */
vmadr		virtop = 2;
struct	vmbuf	*vmhead = &vmpool[0];
struct	vmbuf	vmpool[] = {
	{ &vmpool[1], -1 },
	{ &vmpool[2], -1 },
	{ &vmpool[3], -1 },
	{ &vmpool[4], -1 },
	{ &vmpool[5], -1 },
	{ &vmpool[6], -1 },
	{ &vmpool[7], -1 },
	{ &vmpool[8], -1 },
	{ &vmpool[9], -1 },
	{ &vmpool[10], -1 },
	{ &vmpool[11], -1 },
	{ 0, -1}
};

/*
 * The following variables all have initial values of zero.  We need
 * not explicitly initialize them if the loader we use has a BSS segment.
 */
#ifdef NOBSS

int		ch = 0;
uns		condlev = 0;
uns		curaln = 0;
uns		curatr = 0;
struct	octab	*curdef = 0;
exprval		curloc = 0;
struct	operand	curop = { 0 };
uns		cursec = 0;
int		curxpl = 0;
char		datstr[26] = 0;
uns		deflev = 0;
char		eflg = 0;
uns		errct = 0;
struct	psframe	iilexeme = { 0 };
int		iilset = 0;
int		iilsym = 0;
struct	psframe	iips[IISIZ] = 0;
struct	input	*infp = 0;
int		instk[INSIZ] = 0;
vmadr		label = 0;
char		labstr[SYMSIZ+1] = 0;
char		lflag = 0;
uns		linect = 0;
char		llerr[LLERR+1] = 0;
char		llfull = 0;
char		llloc[LLLOC+1] = 0;
char		llobj[LLOBJ+1] = 0;
char		llseq[LLSEQ+1] = 0;
char		llsrc[SLINSIZ+2] = 0;
uns		mexct = 0;
exprval		nxtloc = 0;
uns		nxtsec = 0;
char		objbuf[OBJSIZ] = 0;
char		objtyp = 0;
struct	octab	*ochtab[1<<OHSHLOG] = 0;
struct	octab	*opcode = 0;
char		opcstr[SYMSIZ+1] = 0;
uns		pagect = 0;
char		parsing = 0;
char		pass2 = 0;
char		pflag = 0;
int		prevsem = 0;
char		*prname = 0;
char		rptct = 0;
uns		rptlev = 0;
vmadr		rptstr = 0;
int		savlen = 0;
char		savstr[STRSIZ+1] = 0;
struct	section	sectab[SECSIZ] = 0;
char		sline[SLINSIZ+2] = 0;
char		*srcfile = 0;
vmadr		syhtab[1<<SHSHLOG] = 0;
char		titl1[TITSIZ+1] = 0;
char		titl2[TITSIZ+1] = 0;
char		tokstr[STRSIZ+1] = 0;
int		toktyp = 0;
exprval		tokval = 0;
uns		truelev = 0;
char		uext = 0;
char		uflg = 0;
int		vmfd = 0;
uns		warnct = 0;
char		xflag = 0;

#ifdef STATS
uns		ashct = 0;
uns		chnct = 0;
char		stats = 0;
uns		sylct = 0;
uns		symct = 0;
uns		vmgct = 0;
uns		vmrct = 0;
uns		vmwct = 0;
#endif STATS

#endif NOBSS
