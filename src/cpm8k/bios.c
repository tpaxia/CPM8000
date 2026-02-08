#ifdef loader
#define LOADER 1
#endif
/*=======================================================================*/
/*+---------------------------------------------------------------------+*/
/*|									|*/
/*|     CP/M-8000(tm) BIOS for the OLIVETTI M20 (Z8000)			|*/
/*|									|*/
/*|     Copyright 1984, Digital Research Inc.				|*/
/*|									|*/
/*+---------------------------------------------------------------------+*/
/*=======================================================================*/

/*-------------------------*/
/* Compilation information */
/*-------------------------*/
/*----------------------------------------------------------------------------*/
/*To compile bios.c for cpmldr.sys the command is: zcc -c -M1 -dLOADER bios.c */
/*This conditionally compiles bios.c leaving unrequired code out of the object*/
/*file.									      */
/*----------------------------------------------------------------------------*/
/* The normal bios compile command for cpm.sys is: zcc -c -M1 bios.c 	      */
/* This will provide the full functionallity of the bios in the object file   */
/*---------------------------------------------------------------------------*/
/* By compiling bios.c with the command : zcc -c -M1 -dTRANSFER bios.c        */
/* You are provided with a bios object that allows the two floppy drives to   */
/* have two different formats. This is left purely as an example for the      */
/* the benefit of porting to a different format and can be modified.	      */
/*----------------------------------------------------------------------------*/
/* By compiling bios.c with the command: zcc -c -M1 -dsect26 bios.c	      */
/* 8" floppy disk support is provided by conditional compilation.	      */
/*----------------------------------------------------------------------------*/

#define BAUD 0   	/* Setting this define to 1 will conditionally compile*/
			/* code setting the tty port to 1200 baud listening   */
			/* XOFF on that port.				      */

/* #define DEBUG 1  */	/* By decommenting this define hard disk debugging */
			/* is enabled. This provides drive, block, and     */
			/* track information to be printed on the console. */

char copyright[] = "Copyright 1984  Digital Research Inc.";

/* HISTORY
**
**	820803	S. Savitzky (Zilog) -- derived from 68000 EXORMACS bios
**
**	830614	F. Zlotnick (Zilog) -- removed initialization of iobyte
**			upon each warmboot.  Changed seldisk to test for
**			overflow of the dphtab (to fix the "dir d:" bug).
**
**	830804	F. Zlotnick (Zilog) -- Added conditional compilation for
**			loader BIOS, which only needs a few of the BIOS
**			functions.  The loader DOES require the definition
**			of a context structure, for transfer of control to
**			the system proper.
**
**	830804	F. Zlotnick (Zilog) -- Changed Disk Parameter Blocks to
**			reflect new bootstrap method.
**
**	830809	F. Zlotnick (Zilog) -- Added escape character to keyboard
**			map, as ctrl'['.
**
**	831205	K. Greenberg (Zilog) -- Fixed disk parameter table for hard
**			drive C to point to dpb3, not dpb2 (80 trk floppy).
**
**	831212	K. Greenberg (Zilog) -- Modified disk parameter tables for
**			hard disk to look more like floppies (fewer sectors
**			but more tracks). This will fix the sector deblocking
**			part of the bios to be compatible with both. Also
**			switched to 4K allocation blocks and 512 direntries.
*/








/************************************************************************/ 
/************************************************************************/ 
/*									*/
/*      I/O Device Definitions						*/
/*									*/
/************************************************************************/ 
/************************************************************************/




/************************************************************************/
/* Define Interrupt Controller constants				*/
/************************************************************************/

/* The interrupt controller is an Intel 8259 left-shifted one bit	*/
/* to allow for the word-alligned interrupt vectors of the Z8000.	*/

/* === I am going to assume that this is set up in the PROM === */


/************************************************************************/ 
/*      Define the two USART ports					*/
/************************************************************************/

/* The USARTs are Intel 8251's */

#define KBD	0xA1		/* Keyboard USART base		*/
#define RS232	0xC1		/* RS-232 terminal		*/

#define SERDATA 0		/* data port offset		*/
#define SERCTRL 2		/* control port offset		*/
#define SERSTAT 2		/* status port offset		*/

#define SERINIT 0x37		/* init (3 times)		*/
#define SERRES  0x40		/* reset			*/
#define SERMODE 0xEE		/* mode (2 stop, even parity	*/
				/*	parity disable, 8 bits	*/
				/*	divide by 16		*/
				/* DUBIOUS.  15??		*/
#define TTYON  0x37		/* cmd (no hunt, no reset, 	*/
				/*	RTS=0, error reset,	*/
				/*	no break, rcv enable,	*/
				/*	DTR=0, xmt enable	*/
#define SERRRDY 0x02		/* RCV ready bit mask		*/
#define SERXRDY 0x01		/* XMT ready bit mask		*/
#define XON	0x11		/* Control- Q			*/
#define XOFF	0x13		/* Control- S			*/

/************************************************************************/ 
/*      Define the counter/timer ports					*/
/************************************************************************/

/* The counter-timer is an Intel 8253 */

#define CT_232	0x121		/* counter/timer 0 -- RS232 baud rate	*/
#define CT_KBD	0x123		/* counter/timer 1 -- kbd baud rate	*/
#define CT_RTC	0x125		/* counter/timer 2 -- NVI (rt clock)	*/
#define CT_CTRL	0x127		/* counter/timer control port		*/

#define CT0CTL	0x36		/* c/t 0 control byte			*/
#define CT1CTL	0x76		/* c/t 1 control byte			*/
#define CT2CTL	0xB4		/* c/t 2 control byte			*/

/* control byte is followed by LSB, then MSB of count to data register	*/
/* baud rate table follows:						*/

#ifndef	LOADER			/* NOT needed by the Loader Bios	*/

int baudRates[10] = {
			1538,	/*    50 */
			 699,	/*   110 */
			 256,	/*   300 */
			 128,	/*   600 */
			  64,	/*  1200 */
			  32,	/*  2400 */
			  16,	/*  4800 */
			   8,	/*  9600 */
			   4,	/* 19200 */
			   2	/* 38400 */
		    };


#endif				/* End Conditional */

/************************************************************************/
/* Define Parallel Port constants					*/
/************************************************************************/

/* The parallel (printer) port is an Intel 8255 */

#define PAR_A	0x81		/* port A data		*/
#define PAR_B	0x83		/* port B data		*/
#define PAR_C	0x85		/* port C data		*/
#define PARCTRL	0x87		/* control port		*/

#define PARBSY	0x02		/* bit one (busy bit) needs to be low */
#define PARFLT  0x10		/* bit five (fault bit) needs to be high */

/************************************************************************/
/************************************************************************/
/*									*/
/* 		PROM AND HARDWARE INTERFACE				*/
/*									*/
/************************************************************************/
/************************************************************************/

/************************************************************************/
/* Define PROM I/O Addresses and Related Constants			*/
/************************************************************************/
/*		SEE BIOSIO.8KN FOR THESE EXTERNALS			*/

extern int disk_io();	/* (char drive, cmd	-- disk I/O		*/
			/*  int  blk_count,				*/
			/*  int  blk_num,				*/
			/*  char *dest) -> int error?			*/

extern crt_put();	/* (char character)	-- put byte to CRT	*/

extern cold_boot();	/* boot operating system			*/

#define DSKREAD	 0	/* disk read command	*/
#define DSKWRITE 1	/* disk write command	*/
#define DSKFMT	 2	/* disk format command	*/
#define DSKVFY	 3	/* disk verify command	*/
#define DSKINIT  4	/* disk init. command	*/


/************************************************************************/
/* Define external I/O routines and addresses				*/
/************************************************************************/
/*		SEE BIOSIF.8KN FOR THESE EXTERNALS			*/

extern output();		/* (port, data: int)	-- output	*/
extern int input();		/* (port: int)		-- input	*/

/************************************************************************/
/* Define external memory management routines				*/
/************************************************************************/
/*		SEE SYSCALL.8KN FOR THESE EXTERNALS			*/

extern mem_cpy();		/* (src, dest, len: long)-- copy data	*/
extern long map_adr();		/* paddr = (laddr: long; space: int)	*/

#define CDATA 0			/* caller data space	*/
#define CCODE 1			/* caller code space	*/
#define SDATA 2			/* system data space	*/
#define SCODE 3			/* system code space	*/
#define NDATA 4			/* normal data space	*/
#define NCODE 5			/* normal code space	*/


/************************************************************************/
/*	System Entry and Stack Pointer					*/
/************************************************************************/

#define	SYSENTRY	0x0b000006L	/* entry point */
#define	SYSSTKPTR	0x0b00bffeL	/* system's stack pointer start */

/************************************************************************/
/*	Memory Region Table						*/
/************************************************************************/

#ifndef LOADER			/* NOT needed for the Loader Bios   */

struct mrt {	int  count;
		struct {long tpalow;
			long tpalen;
		       }	regions[5];
	   }
	memtab = {5,
		  0x0A000000L, 0x10000L,	/* merged    I and D */
		  0x08000000L, 0x10000L,	/* separated I       */
		  0x08000000L, 0x10000L,	/*             and D */
		  0x0B000000L, 0x10000L,	/* accessing I as  D */
		  0x09000000L, 0x10000L		/* ddt debugger sits here */
		 };
#endif				/* End conditional */
#ifdef LOADER			/* NEEDED fr the Loader Bios */

struct mrt {	int  count;
		struct {long tpalow;
			long tpalen;
		       }	regions[1];
	   }
	memtab = {1,
		  0x0B000000L, 0x0C000L,    /* system space: merged I and D */
		 };

struct context			/* Startup context for user's program	*/
	{
		short	regs[14];
		long	segstkptr;
		short	ignore;
		short	FCW;
		long	PC;
	};
struct context context =

	{			/* Regs 0-13 cleared, sp set up below	*/
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		SYSSTKPTR,	/* Loaded system's stack pointer	*/
		0,		/* Ignore: value is zero		*/
		0xD800,		/* FCW: segmented system, VI, NVI set	*/
		SYSENTRY	/* Entry point to system		*/
	};
#endif				/* End conditional */

/************************************************************************/
/* Set Exception Vector entry 						*/
/************************************************************************/

extern long trapvec[];		/* trap vector */

long setxvect(vnum, vval)
int vnum;
long vval;
{
	 register long  oldval;

	oldval = trapvec[vnum];
	trapvec[vnum] = vval;

	return(oldval);	

}


















/************************************************************************/
/************************************************************************/
/*									*/
/*			   CHARACTER I/O				*/
/*									*/
/************************************************************************/
/************************************************************************/

/************************************************************************/
/*      Generic Serial Port I/O Procedures				*/
/************************************************************************/
 

/* define as extern the dirty flag, which is actually defined later	*/
/* on in this file.  Used to flush the buffer at an opportune moment.	*/

extern	int	tbdirty;

serinit(port)
int port;
{
        output(port+SERCTRL, SERINIT);
        output(port+SERCTRL, SERINIT);
        output(port+SERCTRL, SERINIT);

        output(port+SERCTRL, SERRES);
	output(port+SERCTRL, SERMODE);
	output(port+SERCTRL, TTYON);
#if BAUD				/* Conditional for 1200 baud    */
	output(CT_CTRL,CT0CTL);		/* Set baud rate genrator       */
	output(CT_232,baudRates[4]);	/* Modify for different speeds  */
	output(CT_232,0);		/* Set for 1200 baud 	        */
#else
	/* === assume the PROM sets it up. === */
#endif					/* End conditional */
}
 

int serirdy(port)
int port;
{
        return(((input(port+SERSTAT) & SERRRDY) == SERRRDY) ? 0xFF : 0);
}
 
 
char serin(port)
int port;
{
        while (serirdy(port) == 0) ;
        return input(port+SERDATA);
}
 
 
int serordy(port)
int port;
{
	return(((input(port+SERSTAT) & SERXRDY) == SERXRDY) ? 0xFF : 0);
}


serout(port, ch)
int port;
char ch;
{
#if BAUD			/* Conditional for 1200 baud and XOFF */
	while ( ((input(port + SERSTAT) & SERXRDY)
	!= SERXRDY) | ((((input(port + SERDATA))
	& 0x7F) ^ XOFF) == 0));
	output(port + SERDATA, ch);
#else
        while ( (input(port + SERSTAT) & SERXRDY) != SERXRDY) ;
        output(port+SERDATA, ch);
	
#endif				/* End conditional */
}

 
parordy(port)
int port;
{
    int status;
	status = (input(port));
	return (((status & PARBSY) != PARBSY) &&
		((status & PARFLT) == PARFLT) ? 0xFF : 0);
}

parout(port, ch)
int port;
char ch;
{
    register int i, status;

	i = 0;
	do 					
	 {					
	  if (--i == 0)  				 /* only check for   */
		{printstr ("\n\rPrinter Timeout.\n\r");  /* printer ready a  */
		 return;				 /* finite number of */
		} 					 /* times	     */
	 }
	while (!parordy(PAR_B));			 /* if printer ready */
	output (port, ch);				 /* print character  */
	output (PARCTRL, 0x0A);				 /* set strobe low   */
	output (PARCTRL, 0x0B);				 /* set strobe high  */
}

/************************************************************************/
/*	Olivetti keyboard translation table.				*/
/************************************************************************/

#ifndef LOADER			/* NOT needed for the Loader Bios	*/

char kbtran[256] = {

/* Raw key codes for main keypad:

 RE    \    A    B    C    D    E    F    G    H    I    J    K    L    M    N
  O    P    Q    R    S    T    U    V    W    X    Y    Z    0    1    2    3
  4    5    6    7    8    9    -    ^    @    [    ;    :    ]    ,    .    /
*/

/* main keyboard UNSHIFTED. */

0xDD,'\\', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3',
 '4', '5', '6', '7', '8', '9', '-', '^', '@', '[', ';', ':', ']', ',', '.', '/',

/* main keyboard SHIFTED */

0xDE, '|', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_', '!', '"', '#',
 '$', '%', '&','\'', '(', ')', '=', '~', '`', '{', '+', '*', '}', '<', '>', '?',

/* main keyboard CONTROL -- CTL B and C differ from Olivetti. */

0xA0,0x7F,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,
0x0F,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0xE0,0xE1,0xE2,0xE3,
0xE4,0xE5,0xD6,0xE7,0xE8,0xE9,0xEA,0xEB,0x00,0x1B,0x1E,0x1F,0x1D,0xFE,0xFF,0xA4,

/* main keyboard COMMAND */

0xDF,0xF8,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,
0x8E,0x8F,0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0xEC,0xED,0xEE,0xEF,
0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0x13,0x1C,0xFC,0xFD,0x9F,0xF9,0xFA,0xA5,

/* other keys

	    SP   CR   S1   S2 
   KEYPAD    .    0   00    1 
	     2    3    4    5
	     6    7    8    9
	     +    -    *    /
*/

/* other keys UNSHIFTED  -- CR differs from Olivetti */

	   ' ','\r',0x7f,0x08,
	   '.', '0',0xA6, '1',
	   '2', '3', '4', '5',
	   '6', '7', '8', '9',
	   '+', '-', '*', '/',

/* other keys SHIFTED  -- CR differs from Olivetti */

	   ' ','\r',0xA8,0xA9,
	   '.', '0',0xA6,0x1C,
	  0x9A,0x1D,0x9B,0x9C,
	  0x9D,0x1E,0x9E,0x1F,
	  0x2B,0x2D,0x2A,0x2F,

/* other keys CONTROL */

	   ' ','\r',0xA8,0xA9,
	  0xB0,0xB1,0xB2,0xB3,
	  0xB4,0xB5,0xB6,0x1B,
	  0xB8,0xB9,0xBA,0xBB,
	  0xBC,0xBD,0xBE,0xBF,


/* special -- substitute \r for Olivetti's 0xAF. */

	  '\r','\r','\r','\r'
};

#endif				/* End conditional */

/************************************************************************/
/*	specific I/O procedures for use with iobyte	  	        */
/************************************************************************/

/* CRT status, read, write routines */

int crtrs()
{
	return( serirdy(KBD));
}

#ifndef LOADER			/* NOT needed for the Loader Bios */

char crtrd()
{
	return( kbtran[serin(KBD) & 0xff]);
}
#endif				/* End conditional */

#ifdef LOADER			/* Conditional for Loader Bios disable KBD */
#define crtrd nulrd
#endif				/* End conditional */

int crtws()
{
	return(0xFF);
}

#define crtwr crt_put		/* output routine in PROM */


/* TTY status, read, write routines */

int ttyrs()
{
	return(serirdy(RS232));
}

char ttyrd()
{
	return(serin(RS232));
}

int ttyws()
{
	return(serordy(RS232));
}

ttywr(ch)
char ch;
{
	serout(RS232, ch);
}

/* LPT status, output routines */

int lptws()
{
	return (parordy (PAR_B));
}

lptwr(ch)	/* ARGSUSED */
char ch;
{
	parout (PAR_A, ch);
}

/************************************************************************/
/*	generic device names, batch, and null devices			*/
/************************************************************************/

/* the device names are the offset of the proper field in iobyte */

#define CON    0
#define READER 2
#define PUNCH  4
#define LIST   6


/* BATCH status, read, write routines */

#ifndef LOADER			/* NOT needed by the Loader Bios 	*/
int batrs()
{
	int genstat();

	return genstat(READER);
}

char batrd()
{
	int genread();

	return genread(READER);
}

batwr(ch)
char ch;
{
	genwrite(LIST, ch);
}

#endif				/* End Conditional */
#ifdef LOADER			/* NEEDED for the Loader Bios 		*/
#define batrd nulrd
#define batrs nulst
#define batwr nulwr
#endif				/* End conditional */

/* NULL status, read, write routines */

int nulst()
{
	return 0xFF;
}

char nulrd()
{
	return 0xFF;
}

nulwr(ch)		 /* ARGSUSED */
char ch;
{
}

/************************************************************************/
/*	Generic I/O routines using iobyte				*/
/************************************************************************/

/*
** IObyte itself.
*/

char iobyte = 0x41;;

/*
** Device operation tables.  DEVINDEX is the index into the
** table appropriate to a device (row) and its iobyte index (column)
**
**	nonexistent devices are mapped into NUL.
*/

#define DEVINDEX (((iobyte>>dev) & 3) + (dev * 2) )


int (*sttbl[16])() = {
	ttyrs, crtrs, batrs, nulst,	/* con    */
	ttyrs, nulst, nulst, nulst, 	/* reader */
	ttyws, nulst, nulst, nulst, 	/* punch  */
	ttyws, crtws, lptws, nulst	/* list	  */
};

char (*rdtbl[16])() = {
	ttyrd, crtrd, batrd, nulrd,
	ttyrd, nulrd, nulrd, nulrd,
	nulrd, nulrd, nulrd, nulrd,
	nulrd, nulrd, nulrd, nulrd
};

int (*wrtbl[16])() = {
	ttywr, crtwr, batwr, nulwr,
	nulwr, nulwr, nulwr, nulwr,
	ttywr, nulwr, nulwr, nulwr,
	ttywr, crtwr, lptwr, nulwr
};

/* 
** the generic service routines themselves
*/

int genstat(dev)
int dev;
{
	return( (*sttbl[DEVINDEX])() );
}

int genread(dev)
int dev;
{
	return( (*rdtbl[DEVINDEX])() );
}

genwrite(dev, ch)
int dev;
char ch;
{
	(*wrtbl[DEVINDEX])(ch);
}


 
/************************************************************************/ 
/*      Error procedure for BIOS					*/
/************************************************************************/

bioserr(errmsg)
register char *errmsg;
{
        printstr("\n\rBIOS ERROR -- ");
        printstr(errmsg);
        printstr(".\n\r");
        while(1);
}
 
printstr(s)     /* used by bioserr */
register char *s;
{ 
        while (*s) {crtwr(*s); s += 1; };
}


#ifdef DEBUG		/* Conditional for Disk Debugging Hex output */
puthexd(i)	/* put a hex digit to crt */
int i;
{
	i &= 0xf;
	if (i < 10)
		crtwr(i + '0');
	else
		crtwr(i + 'a' - 10);
}

puthexv(i)	/* put an int in hex */
int i;
{
	puthexd(i >> 12);
	puthexd(i >> 8);
	puthexd(i >> 4);
	puthexd(i);
}
#endif			/* End conditional */


/************************************************************************/
/************************************************************************/
/*									*/
/*			    DISK I/O					*/
/*									*/
/************************************************************************/
/************************************************************************/


/************************************************************************/
/* BIOS  Table Definitions						*/
/************************************************************************/

struct dpb
{
	int	spt;		/* sectors per track			*/
	char	bsh;		/* block shift = log2(blocksize/128)	*/
	char	blm;		/* block mask  = 2**bsh - 1		*/
	char	exm;		/* extent mask				*/
	char	dpbjunk;	/* 	dummy field to allign words	*/
	int	dsm;		/* size of disk less offset, in blocks	*/
	int	drm;		/* size of directory - 1		*/
	char	al0;		/* reservation bits for directory	*/
	char	al1;		/* ...					*/
	int	cks;		/* size of checksum vector = (drm+1)/4	*/
	int	off;		/* track offset for OS boot		*/
	char	psh;		/* log2(sectorsize/128)			*/
	char	psm;		/* physical size mask = 2**psh - 1	*/
};



struct dph
{
	char	*xltp;		/* -> sector translation table		*/
	int	 dphscr[3];	/* scratchpad for BDOS			*/
	char	*dirbufp;	/* -> directory buffer (128 bytes)	*/
struct	dpb	*dpbp;		/* -> disk parameter block		*/
	char	*csvp;		/* -> software check vector (cks bytes)	*/
	char	*alvp;		/* -> alloc vector ((dsm/8)+1 bytes)	*/
};




/************************************************************************/
/*	Disk Parameter Blocks						*/
/************************************************************************/

/*
** CP/M assumes that disks are made of 128-byte logical sectors.
**
** The Olivetti uses 256-byte sectors on its disks.  This BIOS buffers
** a track at a time, so sector address translation is not needed.
**
** Sample tables are included for several different disk sizes.
*/

/* === Olivetti has 3 floppy formats & a hard disk === */

#define SECSZ 128	/* CP/M logical sector size			*/
#define TRKSZ  32	/* track size for floppies, 1/2 track sz for hd	*/
#define PSECSZ 256	/* Olivetti physical sector size		*/
#define PTRKSZ 16	/* physical track size				*/
#ifndef	TRANSFER	/* Conditional for Normal bios 			*/
#define MAXDSK  3	/* max. number of disks				*/
#endif			/* End conditional */
#ifdef	TRANSFER	/* Tranfer Conditional needs an extra dpb define*/
#define	MAXDSK	4	/* Disk 4 is a pseudonym for disk 2, with	*/
#endif			/*   an old-style dpb to rescue those files.	*/
			/* End conditional */

/*****   spt, bsh, blm, exm, jnk,   dsm, drm,  al0, al1, cks, off, psh, psm */

struct dpb dpb0=	/* --- 1 side, 16*256 sector, 35 track.  140kb --- */
	{ 32,   4,  15,   1,   0,    64,  63, 0xC0,   0,  16,   3};
struct dpb dpb1=	/* --- 2 side, 16*256 sector, 35 track.  280kb --- */
	{ 32,   4,  15,   1,   0,   134,  63, 0xC0,   0,  16,   3};
struct dpb dpb2=	/* --- 2 side, 16*256 sector, 80 track.  640kb --- */
	{ 32,   4,  15,   0,   0,   314,  63, 0xC0,   0,  16,   3};
struct dpb dpb3=	/* --- 6 side, 32*256 sector, 180 trk.  8640kb --- */
	{ 32,   5,  31,   1,   0,  2154,  511, 0xf0,   0,  0,   3};
#ifdef TRANSFER		/* Conditional Tranfer dpb defined here 	*/
struct dpb dpb4=	/* --- 2 side, 16*256 sector, 35 track.  280kb --- */
	{ 32,   4,  15,   1,   0,   120,  63, 0xC0,   0,  16,   10};
#endif			/* End conditional */	
/*		bls = 2K       dsm = (disk size - 3 reserved tracks) / bls */
/*		bls = 4K for hard disk (8640 - 24) / 4			   */
#ifdef SECT26		/* Conditional for 8" floppy drives	*/

/* === The Olivetti does not have 26-sector disks, but many people do.
**     The following parameter blocks are provided for their use.
*/

struct dpb dpbS=	/* --- 1 side, 26*128 sector, 77 trk --- */
	{ 26,   3,   7,   0,   0,   242,  63, 0xC0,   0,  16,   2};
struct dpb dpbD=	/* --- 1 side, 26*256 sector, 77 trk --- */
	{ 52,   4,  15,   0,   0,   242,  63, 0xC0,   0,  16,   2};

#endif			/* End conditional */

/************************************************************************/
/*	BDOS Scratchpad Areas						*/
/************************************************************************/

char	dirbuf[SECSZ];


char	csv0[16];
char	csv1[16];
char	csv2[32];
#ifdef TRANSFER		/* For Transfer conditional */
char	csv3[16];
#endif			/* End conditional */


char	alv0[32];	/* (dsm0 / 8) + 1	*/
char	alv1[32];	/* (dsm1 / 8) + 1	*/
char	alv2[2002];	/* (dsm2 / 8) + 1	*/
#ifdef TRANSFER		/* For Transfer conditional */
char	alv3[32];
#endif			/* End conditional */

 

/************************************************************************/
/* Sector Translate Table						*/ 
/************************************************************************/



#ifdef SECT26		/* Conditional for 8" floppy drives */

/* === The Olivetti does not have 26-sector disks, but many people do.
**     The following translate table is provided for their use.
*/

char	xlt26[26] = {  1,  7, 13, 19, 25,  5, 11, 17, 23,  3,  9, 15, 21,
	 	       2,  8, 14, 20, 26,  6, 12, 18, 24,  4, 10, 16, 22 };

#endif			/* End conditional */

char	xlt16[32] = { 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
		     17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32};

 
/************************************************************************/
/* Disk Parameter Headers						*/
/*									*/
/* Three disks are defined: dsk a: diskno=0, drive 0			*/
/*			    dsk b: diskno=1, drive 1			*/
/*			    dsk c: diskno=2, drive 10			*/
/************************************************************************/

#ifndef TRANSFER	/* Normal bios dph conditional */
struct dph dphtab[3] =
	{ {xlt16, {0, 0, 0}, dirbuf, &dpb1, csv0, alv0},    /*dsk a*/
	  {xlt16, {0, 0, 0}, dirbuf, &dpb1, csv1, alv1},    /*dsk b*/
	  {xlt16, {0, 0, 0}, dirbuf, &dpb3, csv2, alv2},    /*dsk c*/
	};
#endif			/* End conditional */

#ifdef TRANSFER		/* Trnafer conditional with extra dph */
struct dph dphtab[4] =
	{ {xlt16, {0, 0, 0}, dirbuf, &dpb1, csv0, alv0},    /*dsk a*/
	  {xlt16, {0, 0, 0}, dirbuf, &dpb1, csv1, alv1},    /*dsk b*/
	  {xlt16, {0, 0, 0}, dirbuf, &dpb3, csv2, alv2},    /*dsk c*/
	  {xlt16, {0, 0, 0}, dirbuf, &dpb4, csv3, alv3},    /*dsk d*/
	};
#endif			/* End conditional */

/************************************************************************/
/*	Currently Selected Disk Stuff					*/
/************************************************************************/

int settrk, setsec, setdsk;	/* track, sector, disk #		*/
long setdma;			/* dma address with segment info: long	*/


char	trkbuf[TRKSZ * SECSZ];	/* track buffer		*/
int	tbvalid = 0;		/* track buffer valid	*/
int	tbdirty = 0;		/* track buffer dirty	*/
int	tbtrk;			/* track buffer track #	*/
int	tbdsk;			/* track buffer disk #	*/
int	dskerr;			/* disk error 		*/


/************************************************************************/
/*	Disk I/O Procedures						*/
/************************************************************************/



dskxfer(dsk, trk, bufp, cmd)	/* transfer a disk track */
register int  dsk, trk, cmd;
register char *bufp;
/*
	     This is a handy place to keep notes on Olivetti block
	numbering. For a floppy, bits 3-0 are sector, bit 4 is side,
	and high-order bits are track. We define a floppy to have
	twice as many sectors as there are on a track; thus, the
	sector number overflows to the side bit and all is well. On
	the hard disk, bits 4-0 are sector (there are 32 per track),
	and the high-order bits are (track*6)+surface, where surface
	is in the range 0..5. To make the indexing of trkbuf consistent,
	we define a hard disk to have only 32 logical (16 physical)
	sectors per track, like a floppy. Thus we will transfer only
	half a track to/from the buffer at a time, and the logical
	sector number will overflow into the real high-order bit of
	the sector number. This works because we will always move
	half a track at a time. The tracks and surfaces simply take
	care of themselves, incrementing through the surfaces and
	effectively minimizing seeks.
*/
{
	int blknum;

	if (dsk==2)
		dsk = 10;	/* convert hard disk drive # */
#ifdef TRANSFER			/* Conditional reasignment for Transfer */
	if(dsk==3) dsk = 1;	/* for transfer disks	*/
#endif				/* End conditional */
	dskerr=0;		/* assume no error	     */
				/* do transfer		     */
#ifdef DEBUG			/* Conditional DEBUG output  */
	blknum = trk*PTRKSZ;
	printstr("\nxfer block ");
	puthexv(blknum);
	printstr(" unit ");
	puthexd(dsk);
	printstr(" track ");
	puthexv(trk);
	if (cmd == DSKREAD)
		printstr(" read");
	else
		printstr(" write");
	crtwr(10); crtwr(13);
#endif				/* End conditional */
	if (0 != disk_io(dsk, cmd, PTRKSZ, trk*PTRKSZ, map_adr((long)bufp,0)))
		dskerr=1;
	}


#define wrongtk ((! tbvalid) || (tbtrk != settrk) || (tbdsk != setdsk))
#define gettrk	if (wrongtk) filltb()

#ifndef LOADER			/* NOT needed for Loader Bios */

flush()
{

	if ( tbdirty && tbvalid ) dskxfer(tbdsk, tbtrk, trkbuf, DSKWRITE);
	
	tbdirty = 0;
	
}


#endif				/* End conditional */


filltb()
{

#ifndef LOADER			/* NOT needed by Loader Bios */
	if ( tbvalid && tbdirty ) flush();
#endif				/* End conditional */
	dskxfer(setdsk, settrk, trkbuf, DSKREAD);
		
	tbvalid = 1;
	tbdirty = 0;
	tbtrk	= settrk;
	tbdsk	= setdsk;

}


dskread()
{
	 register char	*p;

	gettrk;
	p = &trkbuf[SECSZ * (setsec-1)];

	/* transfer between memory spaces.  setdma is physical address */

	mem_cpy(map_adr((long)p, CDATA), setdma, (long)SECSZ);

	return(dskerr);
}

#ifndef LOADER			/* NOT needed by Loader Bios it doesn't write */

dskwrite(mode)
char mode;
{
	 register char	*p;

	gettrk;
	p = &trkbuf[SECSZ * (setsec-1)];

	/* transfer between memory spaces.  setdma is physical address */

	mem_cpy(setdma, map_adr((long) p, CDATA), (long)SECSZ);
	tbdirty = 1;
	if ( mode == 1 ) flush();
	return(dskerr);
}

#endif				/* End conditional */

char sectran(s, xp)
int	 s;
char	*xp;
{
	if (xp != 0) return xp[s]; else return s;
}


struct dph *seldisk(dsk, logged)
register char dsk;
    char logged;
{
	register struct dph *dphp;

	if (dsk > MAXDSK) return(0L);
	setdsk = dsk;
	dphp = &dphtab[dsk];
	if (dphp >= dphtab + (sizeof(dphtab)/sizeof(struct dph)) ) return(0L);
	if ( ! logged )
	{

		/* === disk not logged in. select density, etc. === */

	}
	return(dphp);
}






















/************************************************************************/
/************************************************************************/
/*									*/
/*      		    BIOS PROPER					*/
/*									*/
/************************************************************************/
/************************************************************************/

 
 
 
biosinit()
{
#ifdef DEBUG		/* Conditional banner for DEBUG */
	printstr("\r\nCP/M-8000:  Olivetti M20 BIOS DEBUG"); 
#endif			/* End conditional */
        /* serinit(KBD);*/	/* DON'T init keyboard serial port	*/
        serinit(RS232);		/* init rs232 serial port		*/

	tbvalid = 0;		/* init disk flags			*/
	tbdirty = 0;

		/* Following reset of iobyte on each warm boot has been */
		/* removed, so that STAT can reassign devices.  iobyte	*/
		/* is now initialized on cold boot only.		*/
	/* iobyte = 0x41; */	/* con, list = CRT; rdr, punch = TTY	*/
}
 
/* In the LOADER bios, the main routine is called "bios", not "_bios" */
#ifdef LOADER			/* Loader Bios conditional */
#define	_bios	bios
#endif				/* End conditional */
 
long _bios(d0, d1, d2)
int	d0;
long	d1, d2;
{

	switch(d0)
	{
		case 0:				/* INIT		*/	
			biosinit();
			break;

#ifndef LOADER		/* Normal Bios use */
		case 1:				/* WBOOT	*/	
			wboot();
			break;

#endif			/* End conditional */

		case 2:				/* CONST	*/	
			return(genstat(CON));
			break;

		case 3:				/* CONIN	*/ 
			return(genread(CON));
			break;

		case 4:				/* CONOUT	*/ 
			genwrite(CON, (char)d1);
			break;

#ifndef LOADER		/* Normal Bios use */

		case 5:				/* LIST		*/	
			genwrite(LIST, (char)d1);
			break;

		case 6:				/* PUNCH	*/ 
			genwrite(PUNCH, (char)d1);
			break;

		case 7:				/* READER	*/	
			return(genread(READER));
			break;

		case 8:				/* HOME		*/	
			settrk = 0;
			break;

#endif			/* End conditional */

		case 9:				/* SELDSK	*/
			return((long)seldisk((char)d1, (char)d2)); 
			break;

		case 10:			/* SETTRK	*/
			settrk = (int)d1;
			break;

		case 11:			/* SETSEC	*/
			setsec = (int)d1;
			break;

		case 12:			/* SETDMA	*/
			setdma = d1;
			break;

		case 13:			/* READ	        */
			return(dskread());
			break;

#ifndef LOADER		/* Normal Bios use */
		case 14:			/* WRITE	*/
			return(dskwrite((char)d1));
			break;

		case 15:			/* LISTST	*/
			return(genstat(LIST));
			break;

#endif			/* End conditional */

		case 16:			/* SECTRAN	*/ 
			return(sectran((int)d1, (char*)d2));
			break;

		case 18:			/* GMRTA	*/ 
			return((long)&memtab);
			break;

#ifndef LOADER		/* Normal Bios use */
		case 19:			/* GETIOB	*/ 
			return((long)iobyte);
			break;

		case 20:			/* SETIOB	*/ 
			iobyte = (char)d1;
			break;

		case 21:			/* FLUSH	*/ 
			flush();
			return((long)dskerr);
			break;

#endif			/* End conditional */

		case 22:			/* SETXVECT	*/
			return(setxvect((int)d1, d2));
			break;



	} /* end switch */

	return(0);


} /* end bios procedure */ 
 
 
 
/* End of C Bios */
 */
