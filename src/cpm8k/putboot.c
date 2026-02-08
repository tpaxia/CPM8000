/*=======================================================================*/
/*+---------------------------------------------------------------------+*/
/*|									|*/
/*|     CP/M-Z8K(tm) Bootstrap Writer for the OLIVETTI M20 (Z8000)	|*/
/*|									|*/
/*|     Copyright 1984, Digital Research Inc.				|*/
/*|									|*/
/*+---------------------------------------------------------------------+*/
/*=======================================================================*/

char *copyrt = "CP/M-Z8K(tm) Ver. 1.1, Copyright 1984, Digital Research Inc.";
char *serial = "XXXX-0000-654321";


/* HISTORY
**
**	830801	F. Zlotnick (Zilog) -- written
**	840524  rfw modified includes
*/

#include "portab.h" 
#include "osif.h"			/* cpm.h and bdos.h replaces with   */	
#include "stdio.h"			/* osif.h  03-15-84 rfw		    */
#include "bdos.h"
#include "xout.h"

#define CDATA	0			/* Parameter for map_adr()	    */
#define DIRSEC	1			/* Parameter for BIOS Write call    */

#define SETTRK	10			/* BIOS Function 10 = Set Track	    */
#define SETSEC	11			/* BIOS Function 11 = Set Sector    */
#define BSETDMA	12			/* BIOS Function 12 = Set DMA Addr  */
#define WSECTOR	14			/* BIOS Function 14 = Write Sector  */

XADDR	physdir;			/* Segmented address of dirbuf	    */

struct	dpbs	idpb;			/* Disk Parameter Block		    */
struct	bios_parm ibp;			/* BIOS param block for BDOS call 50*/
XADDR	physibp;			/* physical address of ibp structure*/

extern	long	map_adr();		/* Function to return physical addr */

#define	BPLS	128			/* Bytes per logical sector */
#define	BPS	256			/* Bytes per sector */
#define	BPS0	128			/* Bytes per sector, trk 0 */
#define	SPT	16			/* sectors per track */
#define	LSPT	32			/* Logical sectors per track */
#define	SYSTRKS	2			/* Number of boot tracks */
#define	SYSSIZE	SPT*BPS*SYSTRKS		/* Max size of bootstrap */
#define	STARTRK	1			/* Track number to start on */

FILE	*fin;

char	syscode[SYSSIZE];		/* Hold the entire bootstrap here! */
char	*system = "CPMLDR.SYS";		/* Name of the prog to boot */

#ifdef OLIVETTI
extern	char	T0S0[];			/* Track 0 Sector 0 */
extern	char	T1S0[];			/* Track 1 Sector 0 */
extern	char	T1S1[];			/* Track 1 Sector 1 */
#define	HDRSIZE	24			/* # bytes in track 1 sect 1 header */
#endif

struct	x_hdr	xh;
struct	x_sg	xs;
int	dsknum;			/* Drive number 0-15 = A-P  */

main(argc,argv)
int	argc;
char	*argv[];
{
	register int	i, j, c;
	register char	*p;
	long	fsize;
	int	curdsk;				/* Good to remember, & reset*/

	if(argc != 3) usage();
	system = *++argv;
	if( (dsknum = **++argv - 'a') < 0 || dsknum > 15) {
		printf("putboot: Illegal drive code %c\n", *argv[0]);
		exit(1);
	}
	curdsk = _ret_cdisk();
	_get_dpb(map_adr((long) &idpb, CDATA));	/* Physaddr of idpb */
	if( (fin = fopenb(system, "r")) == NULL) {
		printf("putboot: Can't open bootstrap file %s\n", system);
		exit(1);
	}
	fsize = 0L;

		/* read file header */

	p = (char *) &xh;
	for (i = 0; i < sizeof(xh); i++)
		*p++ = (char) getc(fin);

		/* read and count segment headers to get file size */
	for (i = 0; i < xh.x_nseg; i++) {
		p = (char *) &xs;
		for( j = 0; j < sizeof(xs); j++)
			*p++ = (char) getc(fin);		
		if( xs.x_sg_typ != X_SG_BSS && xs.x_sg_typ != X_SG_STK)
			fsize += xs.x_sg_len;
	}
	if (fsize > SYSSIZE) {
		printf("Bootstrap too big\n");
		exit(1);
	}
	else
		printf("Bootstrap file is %ld bytes\n", fsize);
	p = syscode;
#ifdef OLIVETTI
	for( i = 0; i < BPS; i++) *p++ = T1S0[i];
	for( i = 0; i < HDRSIZE; i++) *p++ = T1S1[i];
#endif
	while(fsize--) {
		if( (c = getc(fin)) == EOF) {
			printf("Unexpected EOF in %s, %ld left\n",system,fsize);
			exit(1);
		}
		*p++ = c;
	}

/*
** At this point, the entire bootstrap program code and data has been loaded
**  into the array named "syscode", preceded by a bunch of PCOS garbage
**  which the Olivetti boot PROM expects to find there.  Now we use direct
**  BIOS calls to write the syscode array out to the proper area on disk.
**  For the Olivetti, this is tracks 1 and 2, since track 0 is special.
*/

	putboot(syscode);
	printf("Bootstrap has been written.\n");
	_sel_disk(curdsk);			/* reselect original disk*/
}

putboot(code)
char	*code;
{
	register int	i;			/* Handy index		    */
	register int	nlsecs;			/* # logical sectors in boot*/
	register char	*p;			/* ptr to next part of code */

	int	track;				/* Current track	    */
	int	sector;				/* Current sector	    */

	physibp = map_adr( (long) &ibp, CDATA );
	nlsecs = SYSTRKS * LSPT;		/* size / log secs per trk  */

			/* Pause for the user to insert disk */
/*	pause(drvname); */

#ifdef OLIVETTI
		/* Olivetti Track 0 Sector 0 is special */
	_sel_disk(dsknum);		/* select as current disk */
	putblk(0, 0, T0S0);
#endif
	p = code;
	for(i = 0; i < nlsecs; i++) {
		track = STARTRK + i/LSPT;
		sector = i%LSPT;
		putblk(track, sector, p);
		p += BPLS;
	}
}

/*
 * Function to select a given track for writing on, on the current disk.
 * Makes use of the BDOS direct BIOS call to issue Bios function 10.
 */
settrk(n)
int	n;
{
	ibp.req = SETTRK;		/* BIOS request number 10 */
	ibp.p1 = (long) n;		/* parameter = track # */
	_bios_call( physibp );		/* Pass seg ibp address */
}

/*
 * Function to put block i of the boot track.
 */
putblk(trk, sec, addr)
int	trk, sec;
char	*addr;
{
	register int	n;

	_sel_disk(dsknum);		/* select as current disk */
	settrk(trk);

	n = sec + 1;			/* sector number */
	ibp.req = SETSEC;		/* BIOS request number 11 */
	ibp.p1 = (long) n;		/* parameter = sector # */
	_bios_call( physibp );		/* Pass seg ibp address */

		/* Sector is now set; now set dma address. */

	ibp.req = BSETDMA;		/* BIOS Request number 12 */
	ibp.p1 = map_adr( (long) addr, CDATA);
					/* param = seg address of I/O buffer */
	_bios_call( physibp );		/* Call BIOS */

		/* Now can do a write */

	ibp.req = WSECTOR;		/* BIOS Request number 14 */
	ibp.p1 = DIRSEC;		/* Complete write immediately */
	_bios_call( physibp );		/* Do it! */
}

/*
 * If the user invoked us with the wrong number of args...
 */
usage()
{
	printf("Usage: putboot <filename> <drivecode>\n");
	exit(1);
}
 the wrong number of args...
 */
usage()
{
	printf("Usage: putboot <filename> <drivecode>\n");
	exit(1);
}
