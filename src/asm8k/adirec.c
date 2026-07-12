#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"
#include "obj.h"

/*
 * Version 3.4, 8/27/82.  Changed this routine to dircom, and moved
 * direc to the as*.c files.
 *
 * Version 3.3, 6/9/82.  Fixed .REPEAT bug.
 *
 * Version 3.2, 5/22/82.  Fixed listing control.
 */
static	char	ident[] = "@(#)a.direc.c	3.4";

/*
 * dircom - Performs the assembler directives common to all versions.
 */
dircom(dirnum) int dirnum; {

reg	struct	sytab	*syp;
vmadr	sym;
exprval	l;
int	i;
char	cond;
char	*sp;
char	llsave;

	switch(dirnum) {

	case ADABS:	/* .abs */
		setsec(0);
		assign(STLAB,curloc,cursec);
		break;

	case ADALIGN:	/* .align */
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		if(curop.op_rel==RBABS && 0L<=curop.op_val &&
		 curop.op_val<=32L) {
			i = (int)curop.op_val;
			if(i > curaln) curaln = i;
			l = -1L<<i;
			curloc = curloc-l-1&l;
			if(pass2) setorg();
		} else err('E');
		assign(STLAB,curloc,cursec);
		break;

	case ADBLOCK:	/* .block */
		assign(STLAB,curloc,cursec);
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		if(curop.op_rel == RBABS) {
			curloc += curop.op_val;
			if(pass2) setorg();
		} else err('E');
		break;

	case ADBYTE:	/* .byte */
		assign(STLAB,curloc,cursec);
		if(toktyp != TKSPC) goto synerr;
		iilex();
		while(toktyp != TKEOL) {
			if(toktyp == TKSTR) {
				sp = tokstr;  i = tokval;
				while(i-- > 0) emitb(*sp++,0);
				iilex();
			} else {
				expression();
				emitb((uns)curop.op_val,RAA8|curop.op_rel);
			}
			delim();
		}
		break;

	case ADCLIST:	/* .clist */
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		if(curop.op_rel) err('E');
		else condlst = curop.op_val!=0;
		break;

	case ADCOM:	/* .common */
		if(label) {
			syp = (struct sytab *)rfetch(label);
			if(syp->sy_typ==STSEC &&
			 (!pass2||syp->sy_atr&SADP2)) {  /* old section */
				setsec(syp->sy_rel);
			} else {  /* start a new common section */
				newsec();
			}
			curatr |= SECOM;
		} else err('L');
		break;

	case ADEJECT:	/* .eject */
		llfull = linect = 0;
		break;

	case ADELSE:	/* .else */
		if(truelev) truelev--;
		else err('C');
		break;

	case ADEND:	/* .end */
		if(toktyp == TKSPC) {  /* read transfer address */
			iilex();
			expression();
			if(pass2) {  /* output transfer address */
				oflush();  objtyp = OBTRA;
				oputl((long)curop.op_val);
				oputw(curop.op_rel);
			}
		}
		reading = 0;
		break;

	case ADENDIF:	/* .endif */
		if(condlev) {
			condlev--;  truelev--;
		} else err('C');
		break;

	case ADEQU:	/* .equ */
		equ(STLAB);
		break;

	case ADERROR:	/* .error */
		if(toktyp!=TKSPC || token()!=TKSTR) goto synerr;
		sp = tokstr;
		while(*sp != '\0') err(*sp++);
		token();
		break;

	case ADEXIT:	/* .exit */
		if(infp && (infp->in_typ==INMAC||infp->in_typ==INRPT)) popin();
		else err('O');
		break;

	case ADGLOB:	/* .global */
		if(toktyp != TKSPC) goto synerr;
		token();
		while(toktyp == TKSYM) {
			syp = (struct sytab *)wfetch(sym = sylook(tokstr));
			syp->sy_atr |= SAGLO;
			xref(sym,0);
			token();
			delim();
		}
		break;

	case ADIF:	/* .if */
		condlev++;
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		if(curop.op_rel) {
			cond = 0;  err('E');
		} else cond = curop.op_val!=0;
		if(cond) truelev++;
		break;

	case ADINPUT:	/* .input */
		if(toktyp!=TKSPC || token()!=TKSTR) goto synerr;
		if(include(tokstr) == -1) err('E');
		token();
		break;

	case ADLIST:	/* .list */
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		if(curop.op_rel) err('E');
		else {
			if(0<=curop.op_val&&curop.op_val<=255) i = curop.op_val;
			else i = 255;
			if((curlst = infp->in_lst&0377) != 0) curlst--;
			if(i < curlst) curlst = i;
		}
		/*
		 * We list the .list directive itself only if listing was
		 * on before and is still on now.
		 */
		if(!curlst)
			llfull = 0;
		break;

	case ADLONG:	/* .long */
		assign(STLAB,curloc,cursec);
		if(toktyp != TKSPC) goto synerr;
		iilex();
		while(toktyp != TKEOL) {
			expression();
			emitl((long)curop.op_val,RAA32|curop.op_rel);
			delim();
		}
		break;

	case ADMAC:	/* .macro */
		if(*labstr) {
			curdef = oclook(labstr);
			curdef->oc_typ = OTMAC;
			curdef->oc_arg = 0;
			curdef->oc_val = virtop;
			deflev++;
		} else err('L');
		break;

	case ADORG:	/* .org */
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		if(curop.op_rel == cursec) {
			curloc = curop.op_val;
			assign(STLAB,curop.op_val,curop.op_rel);
			if(pass2) setorg();
		} else err('E');
		break;

	case ADREPT:	/* .repeat */
		rptlev++;
		rptstr = virtop;	/* Set now in case of syntax error */
		rptct = 1;
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		/*
		 * The expression may have generated some new xref entries,
		 * changing virtop.  So we set rptstr again to be sure it is
		 * correct.
		 */
		rptstr = virtop;
		if(curop.op_rel!=RBABS || curop.op_val>255L) err('E');
		else rptct = curop.op_val>=0?curop.op_val:0;
		break;

	case ADSECT:	/* .sect */
		if(label) {
			syp = (struct sytab *)rfetch(label);
			if(syp->sy_typ==STSEC &&
			 (!pass2||syp->sy_atr&SADP2)) {  /* old section */
				setsec(syp->sy_rel);
			} else {  /* start a new section */
				newsec();
			}
		} else err('L');
		break;

	case ADSET:	/* .set */
		equ(STVAR);
		break;

	case ADSPACE:	/* .space */
		llsave = llfull;  /* save list/nolist flag */
		llfull = 0;  /* suppress listing of source line */
		if(toktyp != TKSPC) i = 1;
		else {
			iilex();
			expression();
			if(curop.op_rel != RBABS) err('E');
			i = (int) curop.op_val;
		}
		if(i >= linect) linect = 0;  /* eject page */
		else if(i > 0) {  /* space down specified number of lines */
			putline();  /* get rid of source line */
			llfull = llsave;  /* reset list/nolist flag */
			while(--i > 0)  /* note last line put automatically */
				putline();
		}
		break;

	case ADSTITL:	/* .stitle */
		title(titl2);
		break;

	case ADTITLE:	/* .title */
		title(titl1);
		break;

	case ADWARN:	/* .warn */
		if(toktyp!=TKSPC || token()!=TKSTR) goto synerr;
		sp = tokstr;
		while(*sp != '\0') warn(*sp++);
		token();
		break;

	case ADWITHN:	/* .within */
		if(toktyp != TKSPC) goto synerr;
		iilex();
		expression();
		if(curop.op_rel==RBABS && 0L<=curop.op_val &&
		 curop.op_val<=32L) {
			i = (int)curop.op_val;
			if(i < curext) curext = i;
		} else err('E');
		assign(STLAB,curloc,cursec);
		break;

	case ADWORD:	/* .word */
		assign(STLAB,curloc,cursec);
		if(toktyp != TKSPC) goto synerr;
		iilex();
		while(toktyp != TKEOL) {
			expression();
			emitw((uns)curop.op_val,RAA16|curop.op_rel);
			delim();
		}
		break;

	default:
		err('O');  skipeol();
		break;

	}
	return;

synerr:
	err('S');  skipeol();
	return;
}

