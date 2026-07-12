#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"
#include "obj.h"

/*
 * Version 3.2, 8/20/82.  Added the forward reference flag (OFFOR).
 */
static	char	ident[] = "@(#)a.parse.c	3.2";

/*
 * iiaction - Given the state and symbol on the top of the stack, returns the
 * next action, or -1 if a syntax error was detected.
 */
iiaction() {

reg	int	 *pp;			/* ptab pointer */
reg	char	*scp;			/* scntab pointer */
reg	int	sym;			/* lookahead symbol */

	scp = scntab;  pp = iipsp->ps_state;  sym = iipsp->ps_sym&0377;
	if(iipsp->ps_sym&IINFLG) {  /* scan for a nonterminal symbol */
		scp += *--pp-1;
		do {
			pp--;  scp++;
		} while((*scp&0377)!=sym && (*scp&0377)!=IIDSYM);
		if((*scp&0377) == IIDSYM) pp = ntdflt+(unsigned)sym;
	} else {  /* scan for a terminal symbol */
		scp += *pp++;
		while((*scp&0377)!=sym && (*scp&0377)!=IIDSYM) {
			if((*scp&0377) == IIESYM) return(-1);
			pp++;  scp++;
		}
	}
	return(*pp);
}

/*
 * iiparse - Attempts to parse the input stream, and returns 0 if successful.
 * Expects iilexeme to contain the first symbol of the input at call time.
 * Returns -1 on failure to recover from a syntax error, -2 on parse stack
 * overflow.
 */
iiparse() {

reg	int	act;			/* next action to take */
reg	int	*smp;			/* pointer into semtab */
int		semnum;			/* semantic number */

	eflg = prevsem = uflg = 0;
	iipsp = &iips[IISIZ];
	act = ptab[0];
	parsing = 1;
	do {
		/*
		 * Push the current state and lookahead symbol on the stack.
		 */
		if(--iipsp < iips) return(-2);
		iipsp->ps_state = ptab+(act&~IIXFLG);
		iipsp->ps_sym = iilexeme.ps_sym;
		iipsp->ps_val0 = iilexeme.ps_val0;
		iipsp->ps_val1 = iilexeme.ps_val1;
		/*
		 * Scan the tables to determine the next action.
		 */
		if((act = iiaction()) == -1) return(-1);
		/*
		 * Perform a transition or start a reduction loop, depending
		 * upon the action just found.
		 */
		if(act&IIXFLG) {  /* transition */
			iilex();
		} else {  /* reduction */
			if(act&IIRFLG) {  /* combined read and reduce */
				iilex();
			} else {  /* simple reduction */
				iipsp++;
			}
			do {  /* reduce loop */
				smp = semtab+(act&0377);
				iipspl = iipsp+(act>>8&IILMSK)-1;
				iilsym = *smp>>8&0377;
				iilset = (act&IIAFLG) ? *(smp+1) : 0100000;
				if((semnum = *smp&0377) != 0) {
					if(semnum >= 51) sem51(semnum);
					else sem01(semnum);
					prevsem = semnum;
				}
				iipsp = iipspl;
				iipsp->ps_sym = iilsym|IINFLG;
				act = iiaction();
			} while(!(act&IIXFLG));
		}
	} while(parsing);
	return(iilexeme.ps_sym==TKEOF?0:-1);
}

/*
 * sem01 - Common semantic routines.
 */
sem01(sem) int sem; {

reg	struct	psframe	*p, *pl;
struct	sytab	*syp;
char		*dp, *sp;
int		c, i, j;

	p = iipsp;  pl = iipspl;
	switch(sem) {

	case 1:		/* <s> ::= <operand> */
		parsing = 0;
		break;

	case 2:		/* <expr1> ::= addop <expr1> */
		if(pl->ps_val1 == TVADD) {  /* unary plus */
			pl->ps_val0 = p->ps_val0;
			pl->ps_val1 = p->ps_val1;
		} else {  /* unary minus */
			if(p->ps_val1) goto experr;
			pl->ps_val0 = -p->ps_val0;
			pl->ps_val1 = 0;
		}
		pl->ps_flg = p->ps_flg&OFFOR;
		break;

	case 3:		/* <expr1> ::= unop <expr1> */
		if(p->ps_val1) goto experr;
		pl->ps_val0 = ~p->ps_val0;
		pl->ps_val1 = 0;
		pl->ps_flg = p->ps_flg&OFFOR;
		break;

	case 4:		/* <expr6> ::= <expr6> xorop <expr5> */
		if(pl->ps_val1 || p->ps_val1) goto experr;
		pl->ps_val0 ^= p->ps_val0;
		pl->ps_flg |= p->ps_flg&OFFOR;
		break;

	case 5:		/* <expr2> ::= <expr2> mulop <expr1> */
		if(pl->ps_val1 || p->ps_val1) goto experr;
		switch(p[1].ps_val1) {
		case TVMUL:
			pl->ps_val0 *= p->ps_val0;
			break;
		case TVDIV:
			pl->ps_val0 /= p->ps_val0;
			break;
		case TVMOD:
			pl->ps_val0 %= p->ps_val0;
			break;
		case TVSHL:
			pl->ps_val0 <<= p->ps_val0;
			break;
		case TVSHR:
			pl->ps_val0 >>= p->ps_val0;
			break;
		}
		pl->ps_flg |= p->ps_flg&OFFOR;
		break;

	case 6:		/* <expr3> ::= <expr3> addop <expr2> */
		if(p[1].ps_val1 == TVADD) {  /* addition */
			pl->ps_val0 += p->ps_val0;
			if(pl->ps_val1) {
				if(p->ps_val1) goto experr;
			} else pl->ps_val1 = p->ps_val1;
		} else {  /* subtraction */
			pl->ps_val0 -= p->ps_val0;
			if(pl->ps_val1 == p->ps_val1)
				pl->ps_val1 = 0;
			else if(p->ps_val1) goto experr;
		}
		pl->ps_flg |= p->ps_flg&OFFOR;
		break;

	case 7:		/* <expr5> ::= <expr5> andop <expr4> */
		if(pl->ps_val1 || p->ps_val1) goto experr;
		pl->ps_val0 &= p->ps_val0;
		pl->ps_flg |= p->ps_flg&OFFOR;
		break;

	case 8:		/* <expr> ::= <expr> orop <expr6> */
		if(pl->ps_val1 || p->ps_val1) goto experr;
		pl->ps_val0 |= p->ps_val0;
		pl->ps_flg |= p->ps_flg&OFFOR;
		break;

	case 9:		/* <expr4> ::= <expr3> relop <expr3> */
		switch(p[1].ps_val1) {
		case TVEQ:
			pl->ps_val0 = pl->ps_val1==p->ps_val1 &&
			 pl->ps_val0==p->ps_val0;
			break;
		case TVNE:
			pl->ps_val0 = pl->ps_val1!=p->ps_val1 ||
			 pl->ps_val0!=p->ps_val0;
			break;
		case TVLT:
			if(pl->ps_val1 != p->ps_val1) goto experr;
			pl->ps_val0 = pl->ps_val0<p->ps_val0;
			break;
		case TVGT:
			if(pl->ps_val1 != p->ps_val1) goto experr;
			pl->ps_val0 = pl->ps_val0>p->ps_val0;
			break;
		case TVLE:
			if(pl->ps_val1 != p->ps_val1) goto experr;
			pl->ps_val0 = pl->ps_val0<=p->ps_val0;
			break;
		case TVGE:
			if(pl->ps_val1 != p->ps_val1) goto experr;
			pl->ps_val0 = pl->ps_val0>=p->ps_val0;
			break;
		}
		if(pl->ps_val0) pl->ps_val0 = -1;
		pl->ps_val1 = 0;
		pl->ps_flg |= p->ps_flg&OFFOR;
		break;

	case 10:	/* <primary> ::= ( <expr7> ) */
		pl->ps_val0 = p[1].ps_val0;
		pl->ps_val1 = p[1].ps_val1;
		pl->ps_flg = p[1].ps_flg;
		break;

	case 11:	/* <primary> ::= constant */
		pl->ps_val1 = pl->ps_flg = 0;
		break;

	case 12:	/* <primary> ::= symbol */
		xref((vmadr)p->ps_val1,0);
		syp = rfetch((vmadr)p->ps_val1);
		if(syp->sy_typ == STSEC) goto experr;
		if(syp->sy_typ==STUND || syp->sy_rel==RBUND ||
		 syp->sy_typ==STVAR&&pass2&&!(syp->sy_atr&SADP2)) {
			err('U');  uflg = 1;
			pl->ps_val0 = pl->ps_val1 = 0;
			pl->ps_flg = OFFOR;
		} else {
			pl->ps_val0 = syp->sy_val;
			pl->ps_val1 = syp->sy_rel;
			pl->ps_flg = pass2&&!(syp->sy_atr&SADP2) ? OFFOR : 0;
		}
		break;

	case 13:	/* <primary> ::= $ */
		pl->ps_val0 = curloc;
		pl->ps_val1 = cursec;
		pl->ps_flg = 0;
		break;

	case 14:	/* <string1> ::= string */
		i = savlen = p->ps_val0;  sp = tokstr;  dp = savstr;
		while(i-- > 0) *dp++ = *sp++;
		*dp = '\0';
		break;

	case 15:	/* <operand> ::= <expr> */
		curop.op_cls = (1L<<OCEXP)|(1L<<(prevsem==10?OCPEX:OCNEX));
		if(eflg) {
			curop.op_val = curop.op_rel = curop.op_flg = 0;
		} else {
			curop.op_val = p->ps_val0;
			curop.op_rel = p->ps_val1;
			curop.op_flg = p->ps_flg;
		}
		break;

	case 16:	/* <expr4> ::= <string1> relop string */
		sp = savstr;  i = savlen;
		dp = tokstr;  j = p->ps_val0;
		while(i>0 && j>0 && (c = *sp++ - *dp++)==0) {
			i--;  j--;
		}
		if(c == 0) c = i-j;  /* c reflects the comparison outcome */
		switch(p[1].ps_val1) {
		case TVEQ:
			pl->ps_val0 = c==0;
			break;
		case TVNE:
			pl->ps_val0 = c!=0;
			break;
		case TVLT:
			pl->ps_val0 = c<0;
			break;
		case TVGT:
			pl->ps_val0 = c>0;
			break;
		case TVLE:
			pl->ps_val0 = c<=0;
			break;
		case TVGE:
			pl->ps_val0 = c>=0;
			break;
		}
		if(pl->ps_val0) pl->ps_val0 = -1;
		pl->ps_val1 = pl->ps_flg = 0;
		break;

	}
	return;

experr:
	err('E');  eflg = 1;
	pl->ps_val0 = pl->ps_val1 = 0;
}

