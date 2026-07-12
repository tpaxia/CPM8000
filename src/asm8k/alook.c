#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

/*
 * Version 3.2, 8/19/82.  Added initialization of the sy_val field
 * when adding a new symbol to the symbol table.  It sometimes
 * came up nonzero, giving undefined externals nonzero values in
 * the object output.
 */
static	char	ident[] = "@(#)a.look.c	3.2";

/*
 * oclook - Returns a pointer to the operation table entry for the
 * specified opcode symbol.  Creates a new entry in the operation
 * table if necessary.
 */
struct octab *
oclook(s) char *s; {

struct	octab	*p, *q, *r;
uns	h;
int	cmp;

	h = hash(s)>>(16-OHSHLOG)&(1<<OHSHLOG)-1;
	p = 0;
	q = ochtab[h];
	while(q && (cmp=symcmp(s,q->oc_str))>0) {  /* follow hash chain */
		p = q;
		q = q->oc_lnk;
	}
	if(q && cmp==0) return(q);  /* found */
	/*
	 * Add a new entry to the table.
	 */
	if((uns)phytop&01) phytop++;  /* force integer alignment */
	r = (struct octab *)palloc(sizeof(struct octab));
	symcpy(r->oc_str,s);
	r->oc_typ = 0;
	r->oc_lnk = q;
	if(p) p->oc_lnk = r; else ochtab[h] = r;
	return(r);
}

/*
 * hash - Given a string, computes a partial hashing function of the string,
 * and returns its value.
 */
uns
hash(s) char *s; {

uns	h;

	h = 0;
	while(*s) h = (h<<1)+*s++;
	return(h*40143);
}

/*
 * sylook - Returns a virtual memory pointer to the symbol table entry
 * for the specified symbol.  Creates a new entry in the symbol table
 * if necessary.
 */
vmadr
sylook(s) char *s; {

struct	aside	*apt, **lpt;
vmadr	p, q, r;
struct	sytab	*qp, *rp;
uns	h;
int	cmp;

#ifdef	STATS
	sylct++;
#endif
	/*
	 * Check the lookaside table to see if the symbol is in it.
	 */
	lpt = &ashead;  apt = ashead;
	while((cmp = symcmp(apt->as_str,s)) && apt->as_lnk) {
		apt = *(lpt = &apt->as_lnk);
	}
	/*
	 * Whether we found a hit or not, move the entry to the front of the
	 * lru chain.
	 */
	*lpt = apt->as_lnk;  apt->as_lnk = ashead;  ashead = apt;
	if(cmp == 0) {  /* hit */
#ifdef	STATS
		ashct++;
#endif
		return(apt->as_sym);
	}
	/*
	 * No luck in the lookaside table, so search the hash chains in
	 * the customary manner.
	 */
	symcpy(ashead->as_str,s);  /* update lookaside table */
	h = hash(s)>>(16-SHSHLOG)&(1<<SHSHLOG)-1;
	p = 0;
	q = syhtab[h];
	while(q && (cmp=symcmp(s,(qp=(struct sytab *)rfetch(q))->sy_str))>0) {
		p = q;
		q = qp->sy_lnk;
#ifdef	STATS
		chnct++;
#endif
	}
	if(q && cmp==0) return(ashead->as_sym = q);  /* found */
	/*
	 * Add a new entry to the table.
	 */
#ifdef	STATS
	symct++;
#endif
	if((uns)virtop&01) virtop++;  /* force integer alignment */
	rp = (struct sytab *)wfetch(r=valloc(sizeof(struct sytab)));
	symcpy(rp->sy_str,s);
	rp->sy_typ = rp->sy_atr = rp->sy_xlk = rp->sy_val = 0;
	rp->sy_lnk = q;
	if(p) ((struct sytab *)wfetch(p))->sy_lnk = r; else syhtab[h] = r;
	return(ashead->as_sym = r);
}
