#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

/*
 * Version 3.3, 7/5/82.  Added hooks for profiling and performance
 * enhancements.
 *
 * Version 3.2, 5/22/82.  Added exit routine to replace the standard one
 * in libc.a.
 */
static	char	ident[] = "@(#)a.misc.c	3.3";


/***********************************************************************
 * ferr - Issues a fatal error message and terminates.
 *	Used a variable number of parameters - replaced by in-line
 *	calls to fprintf, followed everywhere by exit(1).
 *	FZ  9/14/82
 */
/*
 *ferr(arglist) char *arglist; {
 *
 *	fprintf(ERROR,"%r",&arglist);
 *	exit(1);
 *}
 ************************************************************************/

/*
 * getline - Reads the next line of source, and puts it into the global
 * array sline and into the listing buffer lline.
 */
getline() {

reg	struct	input	*rinfp;
reg	int	ch;
reg	char	*slp;
reg	char	*llp;

	scanpt = slp = sline;  llp = llsrc;
	for( ; ; ) {
		if((rinfp = infp) == 0) {  /* end of file */
			sline[0] = -1;
			sline[1] = '\0';
			return;
		}
		if(rinfp->in_cnt-- <= 0) {  /* end of input stack frame */
			fillin();
			continue;
		}
		if((ch = *rinfp->in_ptr++ & 0377) >= 0200 &&
		rinfp->in_typ == INMAC) {  /* push macro argument frame */
			macarg(ch);
			continue;
		}
		ch &= 0177;
		if(ch == '\n')  /* we have a line */
			break;
		if(ch == '\f') linect = 0;  /* page eject */
		else if(slp < &sline[SLINSIZ]) *slp++ = *llp++ = ch;
	}
	*slp++ = *llp++ = '\n';  *slp = *llp = '\0';
	rinfp->in_seq++;
	if(rinfp == (struct input *)instk)
		sprintf(llseq," %4d",rinfp->in_seq);
	else sprintf(llseq,"+%4d",rinfp->in_seq);
	/*
	 * Decide now whether listing of this line is enabled.
	 */
	llfull = curlst && (condlev<=truelev || condlst);
	curxpl = pagect<<6|LLPP-linect+1;
	if(linect == 0) curxpl += 64-LLPP;  /* anticipate form feed */
}

/*
 * palloc - Allocates a block of physical memory of the specified size,
 * and returns a pointer to the block.
 */
char *
palloc(size) uns size; {

static	char	*oldtop;

	oldtop = phytop;  phytop += size;
	while(phytop > phylim) {
		if(sbrk(1024) == -1) {
			fprintf(ERROR,"Out of memory\n");
			exit(1);
		}
		phylim += 1024;
	}
	return(oldtop);
}

/*
 * pgcheck - Checks to see if a new listing page is needed, and starts one
 * if necessary.  Then updates the line counter in anticipation of a line
 * of output.
 */
pgcheck() {

int	i;

	if(linect <= 0) {  /* time for a new page */
		pagect++;
		fprintf(LIST,"\f\n\n%-8s%-48s%s\
        %-48s                Page%4d\n\n",
		 prname,titl1,datstr,titl2,pagect);
		linect = LLPP;
	}
	linect--;
}

/*
 * putline - Outputs the next line of the assembly listing, provided
 * it is pass 2 and there is something to output.
 */
putline() {

static	char	lstfmt[] = "%-3s %-4s %-8s %-5s %s";

	if(pass2) {
		if(*llerr) {  /* put out an error listing line */
			fprintf(ERROR,lstfmt,llerr,llloc, llobj,llseq,llsrc);
		}
		if(lflag && (llfull||*llerr)) {  /* put out assembly listing */
			pgcheck();  /* start a new page if necessary */
			if(*llerr == '\0') {  /* put listing line number */
				sprintf(llerr," %2d",LLPP-linect);
			}
			fprintf(LIST,lstfmt,llerr,llloc,llobj,llseq,llsrc);
		}
	}
	llsrc[0] = '\n';
	*llerr = *llloc = *llobj = *llseq = llsrc[1] = '\0';
	llert = llerr;  llobt = llobj;
}

/*
 * symcmp - Compares two symbols, and returns a number which is:
 *
 *	> 0, if a > b,
 *	== 0, if a == b,
 *	< 0, if a < b.
 */
symcmp(a,b) reg char *a, *b; {

reg	int	i;

	i = SYMSIZ;
	while(--i>=0 && *a==*b++) {
		if(*a++ == '\0') return(0);
	}
	return(i<0 ? 0 : *a-*--b);
}

/*
 * symcpy - Copies one symbol from source to destination.
 */
symcpy(d,s) reg char *d, *s; {

reg	char	i;

	i = SYMSIZ;
	do {
		if((*d++ = *s++) == '\0') return;
	} while(--i > 0);
}
