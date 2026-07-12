#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"
#include "obj.h"

static	char	ident[] = "@(#)a.xref.c	3.1";

/*
 * putxref - Prints out the cross reference listing.
 */
putxref() {

struct	sytab	*syp;
struct	xref	*xrp;
vmadr		sym, xr, xrtail;
uns		llcol;
char		undef;

	strcpy(titl2,"Cross Reference Listing");  linect = 0;
	sysort();
	for(sym=syhtab[0] ; sym!=0 ; sym=syp->sy_lnk) {
		syp = (struct sytab *)rfetch(sym);
		xrtail = syp->sy_xlk;
		undef = syp->sy_typ==STUND || syp->sy_rel==RBUND;
		if(syp->sy_typ==STKEY || syp->sy_typ==STSEC ||
		 undef&&xrtail==0) continue;
		pgcheck();
		fprintf(LIST,"%-8.8s ",syp->sy_str);
		if(undef) fprintf(LIST,"        U");
		else if(syp->sy_atr&SAMUD) fprintf(LIST,"        M");
		else {
			fprintf(LIST,"%8X",(long)syp->sy_val);
			if(syp->sy_rel == RBABS) fputc(' ',LIST);
			else if(syp->sy_rel >= RBEXT) fputc('X',LIST);
			else fputc('\'',LIST);
		}
		if(xrtail != 0) {  /* we have some xref entries */
			xrp = (struct xref *)rfetch(xrtail);
			llcol = 18;
			do {
				xr = xrp->xr_lnk;
				xrp = (struct xref *)rfetch(xr);
				if(llcol > rmarg-8) {  /* start new line */
					fputc('\n',LIST);
					pgcheck();
					fprintf(LIST,"                  ");
					llcol = 18;
				}
				fprintf(LIST," %3d-%2d%c",xrp->xr_pl>>6&0777,
				 xrp->xr_pl&077,xrp->xr_pl&XRDEF?'*':' ');
				llcol += 8;
			} while(xr != xrtail);
			syp = (struct sytab *)rfetch(sym);
		}
		fputc('\n',LIST);
	}
}

/*
 * symerge - Merges the two specified symbol table hash chains, and
 * returns a pointer to the resulting chain.
 */
vmadr
symerge(a,b) vmadr a, b; {

struct	sytab	*ap, *bp;
vmadr		pa, r, t;

	if(a == 0) return(b);
	if(b == 0) return(a);
	/*
	 * Initialize so that the first element of chain a is less than the
	 * first element of chain b.
	 *
	 * CAUTION -- This code requires an lru virtual memory buffer pool
	 * of at least 3 blocks.
	 */
	if(symcmp((ap=(struct sytab *)rfetch(a))->sy_str,
	 (bp=(struct sytab *)rfetch(b))->sy_str) > 0) {  /* exchange a and b */
		t = a;  a = b;  b = t;  ap = bp;
	}
	r = a;  /* initialize result with smallest element */
	/*
	 * Attach successive smallest portions of the chains onto the result
	 * until there is nothing left.
	 */
	do {
		/*
		 * Walk along chain a until its end is reached or it is
		 * no longer the smaller of the two.
		 */
		do {
			pa = a;
		} while((a=ap->sy_lnk)!=0 &&
		 symcmp((ap=(struct sytab *)rfetch(a))->sy_str,
		 (bp=(struct sytab *)rfetch(b))->sy_str)<=0);
		/*
		 * Link b to the end of the result, then exchange the chains
		 * so that a again points to the smaller of the two.
		 */
		((struct sytab *)wfetch(pa))->sy_lnk = b;
		t = a;  a = b;  b = t;  ap = bp;
	} while(b != 0);
	return(r);
}

/*
 * sysort - Sorts the symbol table into a single alphabetical chain whose
 * head is syhtab[0].
 */
sysort() {

uns	halfi, i, j;

	for(i=1<<SHSHLOG ; i>1 ; i=halfi) {
		halfi = i>>1;
		for(j=0 ; j<halfi ; j++) {
			syhtab[j] = symerge(syhtab[j],syhtab[j+halfi]);
		}
	}
}

/*
 * xref - Adds a cross reference entry of the specified type for the
 * specified symbol.
 */
xref(sym,type) vmadr sym; int type; {

struct	sytab	*syp;
struct	xref	*oxp, *nxp;
vmadr		nxr;
int		pl;

	if(!(xflag&&pass2)) return;
	pl = curxpl|type;
	/*
	 * CAUTION -- This code requires an lru virtual memory buffer pool
	 * of at least 3 blocks.
	 */
	syp = (struct sytab *)wfetch(sym);
	if(syp->sy_xlk == 0) {  /* first xref entry for this symbol */
		if((uns)virtop&01) virtop++;  /* force alignment */
		nxp = (struct xref *)wfetch(nxr=valloc(sizeof(struct xref)));
		nxp->xr_lnk = nxr;
	} else {  /* add to existing circular list of xref entries */
		oxp = (struct xref *)wfetch(syp->sy_xlk);
		if(oxp->xr_pl == pl) return;  /* no duplicate xrefs */
		if((uns)virtop&01) virtop++;  /* force alignment */
		nxp = (struct xref *)wfetch(nxr=valloc(sizeof(struct xref)));
		nxp->xr_lnk = oxp->xr_lnk;  oxp->xr_lnk = nxr;
	}
	nxp->xr_pl = pl;
	syp->sy_xlk = nxr;
}
