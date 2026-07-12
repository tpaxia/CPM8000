#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"
#include "asz8k.h"
#include "obj.h"

/*
 * Version 3.4, 8/27/82.  Changes to accomodate 8086 version.
 *
 * Version 3.3, 5/22/82.  Added PDDIR define for predef directory.
 */
static	char	ident[] = "@(#)asz8k.c	3.4";

/*
 * direc - Processes assembler directives which are special to this
 * assembler.  Calls dircom to process other directives.
 */
direc(dirnum) int dirnum; {

	if(dirnum != ADMAC) {  /* look up the label */
		label = *labstr?sylook(labstr):0;
	}
	switch(dirnum) {

	case ADLONG:	/* .long */
		if(curloc & 01)		/* Force word alignment */
			curloc++;
		assign(STLAB,curloc,cursec);
		if(toktyp != TKSPC) goto synerr;
		iilex();
		while(toktyp != TKEOL) {
			expression();
			/*
			 * If we are assembling for the segmented version, and
			 * if the expression is relocatable, we generate a long
			 * segmented address.  Otherwise, we generate a normal
			 * 32-bit value.
			 */
			if(segflg && curop.op_rel!=0)
				emitls((long)curop.op_val, curop.op_rel);
			else
				emitlm((long)curop.op_val, RAA32M|curop.op_rel);
			delim();
		}
		return;

	case ADWORD:	/* .word */
		if(curloc & 01)		/* Force word alignment */
			curloc++;
		assign(STLAB,curloc,cursec);
		if(toktyp != TKSPC) goto synerr;
		iilex();
		while(toktyp != TKEOL) {
			expression();
			/*
			 * For the segmented version, we generate the
			 * offset portion of the expression.  (Note that
			 * if the expression is non-relocatable, its offset
			 * is the same as its value.)  For the non-segmented
			 * version, we simply generate a normal 16-bit value.
			 */
			emitwm((uns)curop.op_val,
			(segflg?RAZOF:RAA16M)|curop.op_rel);
			delim();
		}
		return;

	default:
		dircom(dirnum);
		return;
	}
	return;

synerr:
	err('S');  skipeol();
}

/*
 * emitlm - Emits a long to the object file, MSW first.
 */
emitlm(value, reloc) long value; uns reloc; {

	emitwm((uns)(value>>16), reloc);
	emitwm((uns)value, 0);
}

/*
 * emitls - Emits a long segmented address to the object file.
 */
emitls(value, reloc) long value; uns reloc; {

uns	seg;

	reloc &= RBMSK;
	seg = reloc<=0x7f ? reloc : 0x7f;
	value &= 0xffffL;
	emitlm(((long)(0x80|seg)<<24)|value, RAZLS|reloc);
}

/*
 * emitss - Emits a short segmented address to the object file.
 */
emitss(value, reloc) uns value, reloc; {

uns	seg;

	reloc &= RBMSK;
	seg = reloc<=0x7f ? reloc : 0x7f;
	value &= 0xff;
	emitwm((seg<<8)|value, RAZSS|reloc);
}

/*
 * emitwm - Emits a word to the object file, MSB first.
 */
emitwm(value, reloc) uns value, reloc; {

	emitb(value>>8, reloc);
	emitb(value, 0);
}

/*
 * equ - Handles the .equ and .set directives.  The argument is the type
 * of symbol to define (label or variable).  Some assemblers have special
 * interest code here to allow assigning symbols to registers or other
 * keywords.
 */
equ(symtype) int symtype; {

	if(!label) err('L');
	if(toktyp != TKSPC) goto synerr;
	iilex();
	if(iiparse()!=0 || !(curop.op_cls&1L<<OCEXP)) goto synerr;
	if(!(eflg||uflg) || pass2)
		assign(symtype,curop.op_val,curop.op_rel);
	return;
synerr:
	err('S');  skipeol();
}

/*
 * iilex - Lexical scanner for expression parsing.  Calls token and does
 * further processing as required by the isp parser.
 */
iilex() {

struct	sytab	*syp;

	iilexeme.ps_sym = token();
	iilexeme.ps_val0 = tokval;
	if(iilexeme.ps_sym == TKSYM) {  /* symbol */
		iilexeme.ps_val0 = (exprval)sylook(tokstr);
		syp = rfetch((vmadr)iilexeme.ps_val0);
		if(syp->sy_typ == STKEY) {  /* keyword */
			iilexeme.ps_val0 = syp->sy_val&0377;
			iilexeme.ps_sym = syp->sy_val>>8&0377;
		}
	} else if(iilexeme.ps_sym==TKCOM || iilexeme.ps_sym==TKSPC ||
	 iilexeme.ps_sym==TKEOL) iilexeme.ps_sym = TKEOF;
	iilexeme.ps_val1 = (uns)iilexeme.ps_val0;
	return(iilexeme.ps_sym);
}

/*
 * inops - Reads the operands for a machine instruction and leaves their
 * descriptions in optab.
 */
inops() {

struct	operand	*opp;

	if(toktyp == TKSPC) iilex();
	for(opp=optab ; opp<optab+OPMAX ; opp++) {
		opp->op_cls = 1L<<OCNULL;
		if(toktyp == TKEOL) continue;
		if(iiparse() != 0) {
			err('S');
			skipeol();
			continue;
		}
		if(curop.op_cls & (1L<<OCEXP))
			curop.op_cls |= (1L<<OCX);
		*opp = curop;
		delim();
	}
}

/*
 * instr - Generates the specified machine instruction.
 */
instr(fmp) struct format *fmp; {

long	v,
	dispval,
	immval;
int	displen,
	disprel,
	f,
	i,
	immlen,
	immrel,
	r,
	skel0,
	skel1,
	skel2,
	skel3,
	srel1;

	if(curloc & 01)		/* Force word alignment */
		curloc++;
	label = *labstr?sylook(labstr):0;
	assign(STLAB,curloc,cursec);
	inops();  /* read the instruction operands */
	while(!opmatch(fmp)) {  /* scan for matching format entry */
		if(fmp->fm_flg&FMLAST) goto nomatch;
		fmp++;
	}
	displen = immlen = 0;
	/*
	 * Build up the instruction's fields.
	 */
	skel0 = fmp->fm_skel>>8 & 0xff;
	skel1 = fmp->fm_skel & 0xff;
	skel2 = 0;
	skel3 = fmp->fm_flg & FMNIB7;
	srel1 = 0;
	for(i=0 ; i<OPMAX ; i++) {
		f = optab[i].op_flg;
		r = optab[i].op_rel;
		v = optab[i].op_val;
		switch(fmp->fm_op[i] & OAMSK) {

		case OANIB1:	/* Pack value into nibble 1 */
			if(r!=0 || v<0 || v>15)
				err('E');
			skel0 |= v & 0x0f;
			break;
		
		case OANIB2:	/* Pack value into nibble 2 */
			if(r!=0 || v<0 || v>15)
				err('E');
			skel1 |= v<<4 & 0xf0;
			break;
		
		case OANIB3:	/* Pack value into nibble 3 */
			if(r!=0 || v<0 || v>15)
				err('E');
			skel1 |= v & 0x0f;
			break;
		
		case OANIB5:	/* Pack value into nibble 5 */
			if(r!=0 || v<0 || v>15)
				err('E');
			skel2 |= v & 0x0f;
			break;

		case OANIB6:	/* Pack value into nibble 6 */
			if(r!=0 || v<0 || v>15)
				err('E');
			skel3 |= v<<4 & 0xf0;
			break;

		case OANIB7:	/* Pack value into nibble 7 */
			if(r!=0 || v<0 || v>15)
				err('E');
			skel3 |= v & 0x0f;
			break;
		
		case OABYT1:	/* Pack value into byte 1 */
			skel1 = v;
			srel1 = r;
			break;
		
		case OAX2:	/* Generate indexed addr using nibble 2 */
			skel1 |= (f & OFXMSK) << 4;
			dispval = v;
			disprel = r;
			if(segflg)
				displen = f&OFSHORT ? 2 : 4;
			else
				displen = 2;
			break;

		case OAX3:	/* Generate indexed addr using nibble 3 */
			skel1 |= f & OFXMSK;
			dispval = v;
			disprel = r;
			if(segflg)
				displen = f&OFSHORT ? 2 : 4;
			else
				displen = 2;
			break;
		
		case OABA2:	/* Generate based addr using nibble 2 */
			skel1 |= (f & OFXMSK) << 4;
			dispval = v;
			disprel = r;
			displen = 2;
			break;

		case OABA3:	/* Generate based addr using nibble 3 */
			skel1 |= f & OFXMSK;
			dispval = v;
			disprel = r;
			displen = 2;
			break;

		case OABX2:	/* Generate based-indexed addr using nibble 2 */
			skel1 |= (f & OFXMSK) << 4;
			skel2 |= v;
			break;
		
		case OABX3:	/* Generate based-indexed addr using nibble 3 */
			skel1 |= f & OFXMSK;
			skel2 |= v;
			break;
		
		case OACFLAG:	/* OR condition flag into nibble 2 */
			skel1 |= 1<<v;
			break;
		
		case OANIB3S:	/* Pack into low 3 bits of nibble 3 */
			if(r!=0 || v<0 || v>7)
				err('E');
			skel1 |= v & 0x07;
			break;

		case OANIB3A:	/* AND into nibble 3 */
			skel1 &= 0xf0 | v;
			break;

		case OANIB3D:	/* Decrement and pack into nibble 3 */
			v--;
			if(r!=0 || v<0 || v>15)
				err('E');
			skel1 |= v & 0x0f;
			break;
		
		case OANIB7D:	/* Decrement and pack into nibble 7 */
			v--;
			if(r!=0 || v<0 || v>15)
				err('E');
			skel3 |= v & 0x0f;
			break;

		case OARA7:	/* Generate 7-bit relative address */
			v = curloc+2 - v;
			if(r!=cursec || v&01 || v<0 || v>=(2*128))
				err('E');
			skel1 |= v >> 1;
			break;

		case OARA8:	/* Generate 8-bit relative address */
			v -= curloc+2;
			if(r!=cursec || v&01 || v<(2*-128) || v>=(2*128))
				err('E');
			skel1 |= v >> 1;
			break;

		case OARA12:	/* Generate 12-bit relative address */
			v = curloc+2 - v;
			if(r!=cursec || v&01 || v<(2*-2048) || v>=(2*2048))
				err('E');
			skel0 |= (v>>9) & 0x0f;
			skel1 = (v>>1) & 0xff;
			break;
		
		case OARA16:	/* Generate 16-bit relative address */
			v -= curloc+4;
			if(r!=cursec || v&01)
				err('E');
			/*
			 * We treat this as an immediate because we don't
			 * want a segmented form of address for the segmented
			 * version.
			 */
			immlen = 2;
			immrel = 0;
			immval = v & 0xffff;
			break;
		
		case OAIMM8:	/* Generate 8-bit immediate */
			immlen = 1;
			immrel = r;
			immval = v & 0xff;
			break;
		
		case OAIMM16:	/* Generate 16-bit immediate */
			immlen = 2;
			immrel = r;
			immval = v & 0xffff;
			break;
		
		case OAIMM32:	/* Generate 32-bit immediate */
			immlen = 4;
			immrel = r;
			immval = v;
			break;

		case OASHFT:	/* Generate shift count of 1 or 2 */
			if(r!=0 || v<1 || v>2)
				err('E');
			if(v == 2)
				skel1 |= 2;
			break;
		}
	}
	emitb(skel0, 0);
	emitb(skel1, RAA8|srel1);
	if(fmp->fm_flg & FMSKEL2) {
		if(fmp->fm_flg & FMNEGI)	/* Fake -1 immed. for R shift */
			skel2 = skel3 = 0xff;
		emitb(skel2, 0);
		emitb(skel3, 0);
	}
	if(displen == 2) {
		if(segflg)
			emitss((uns)dispval, disprel);
		else
			emitwm((uns)dispval, RAA16M|disprel);
	} else if(displen == 4)
		emitls((long)dispval, disprel);
	if(immlen == 1) {
		emitb((uns)immval, RAA8|immrel);
		emitb((uns)immval, RAA8|immrel);
	} else if(immlen == 2) {
		if(fmp->fm_flg & FMNEGI) {
			/*
			 * Negate immediate value for right shift instrucions.
			 */
			if(immrel != 0)
				err('E');
			immval = -immval;
		}
		/*
		 * For the segmented version, we generate the
		 * offset portion of the expression.  (Note that
		 * if the expression is non-relocatable, its offset
		 * is the same as its value.)  For the non-segmented
		 * version, we simply generate a normal 16-bit value.
		 */
		emitwm((uns)immval, (segflg?RAZOF:RAA16M)|immrel);
	} else if(immlen == 4) {
		/*
		 * If we are assembling for the segmented version, and if
		 * the expression is relocatable, we generate a long
		 * segmented address.  Otherwise, we generate a normal
		 * 32-bit value.
		 */
		if(segflg && immrel!=0)
			emitls((long)immval, immrel);
		else
			emitlm(immval, RAA32M|immrel);
	}
	return;

nomatch:
	err('O');
}

/*
 * opmatch - Returns 1 if the specified format entry matches the operands
 * in optab, 0 otherwise.
 */
opmatch(fmp) struct format *fmp; {

int	i;

	for(i=0 ; i<OPMAX ; i++) {
		if(!(1L<<(fmp->fm_op[i]&OCMSK)&optab[i].op_cls)) return(0);
	}
	return(1);
}

/*
 * predef - Reads the predefined symbols into the symbol table.
 */
predef() {

struct	format	*fmp;
struct	octab	*ocp;
struct	sytab	*syp;
int	i, val;
char		predef[30];

	/*
	 * Open the predef file and read in the definitions.
	 */
	sprintf(predef,"%s.pd",prname);  /* use local predef file */
	if(include(predef) == -1) {
		fprintf(ERROR,"No PREDEF file (%s)\n",predef);
		exit(1);
	}
	while(token() == TKCON) {  /* read machine instructions */
		if((uns)phytop&01) phytop++;  /* force alignment */
		val = phytop;
		for(;;) {  /* read format table entries */
			fmp = palloc(sizeof(struct format));
			for(i=0 ; i<OPMAX ; i++) {  /* operand descriptors */
				fmp->fm_op[i] = tokval;
				preget(TKSPC);  preget(TKCON);
			}
			fmp->fm_skel = tokval;
			preget(TKSPC);
			fmp->fm_flg = preget(TKCON);
			if(token() != TKEOL) break;
			preget(TKCON);
		}
		fmp->fm_flg |= FMLAST;
		while(toktyp == TKSPC) {  /* read instruction mnemonics */
			preget(TKSYM);  ocp = oclook(tokstr);
			ocp->oc_typ = OTINS;  ocp->oc_val = val;
			token();
		}
		if(toktyp != TKEOL) badpre();
	}
	if(toktyp != TKEOL) badpre();
	while(token() == TKCON) {  /* read assembler directives */
		val = tokval;
		while(token() == TKSPC) {  /* read directive mnemonics */
			preget(TKSYM);  ocp = oclook(tokstr);
			ocp->oc_typ = OTDIR;  ocp->oc_val = val;
		}
		if(toktyp != TKEOL) badpre();
	}
	if(toktyp != TKEOL) badpre();
	while(token() == TKCON) {  /* read predefined symbols */
		val = tokval;
		while(token() == TKSPC) {  /* read symbol mnemonics */
			preget(TKSYM);  syp = wfetch(sylook(tokstr));
			syp->sy_typ = STKEY;
			syp->sy_val = val;
			syp->sy_atr = SADP2;
		}
		if(toktyp != TKEOL) badpre();
	}
	preget(TKEOF);
}

/*
 * sem51 - Parser semantic routines specific to asz8k.
 */
sem51(sem) int sem; {

reg	struct	psframe	*p, *pl;

	p = iipsp;  pl = iipspl;
	switch(sem) {

	case 51:	/* <operand> ::= reg8 */
		curop.op_cls = (1L<<OCREG8);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;

	case 52:	/* <operand> ::= reg16 */
		curop.op_cls = (1L<<OCREG16);
		if(!segflg)
			curop.op_cls |= (1L<<OCREGA);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;

	case 53:	/* <operand> ::= reg32 */
		curop.op_cls = (1L<<OCREG32);
		if(segflg)
			curop.op_cls |= (1L<<OCREGA);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;

	case 54:	/* <operand> ::= reg64 */
		curop.op_cls = (1L<<OCREG64);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;

	case 55:	/* <operand> ::= pound <expr> */
		curop.op_cls = (1L<<OCIMM);
		if(eflg)
			curop.op_val = curop.op_rel = curop.op_flg = 0;
		else {
			curop.op_val = p->ps_val0;
			curop.op_rel = p->ps_val1;
			curop.op_flg = p->ps_flg;
		}
		return;
	
	case 56:	/* <operand> ::= <indexed> */
		curop.op_cls = (1L<<OCX);
		if(eflg)
			curop.op_val = curop.op_rel = curop.op_flg = 0;
		else {
			curop.op_val = pl->ps_val0;
			curop.op_rel = pl->ps_val1;
			curop.op_flg = pl->ps_flg;
		}
		return;
	
	case 57:	/* <operand> ::= @ reg16 */
		curop.op_cls = (1L<<OCIRIO);
		if(!segflg)
			curop.op_cls |= (1L<<OCIR);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;
	
	case 58:	/* <operand> ::= @ reg32 */
		if(segflg)
			curop.op_cls = (1L<<OCIR);
		else
			curop.op_cls = 0;
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;

	case 59:	/* <operand> ::= <basereg> ( pound <expr> ) */
		curop.op_cls = (1L<<OCBA);
		if(eflg)
			curop.op_val = curop.op_rel = curop.op_flg = 0;
		else {
			curop.op_val = p[1].ps_val0;
			curop.op_rel = p[1].ps_val1;
			curop.op_flg = p[1].ps_flg | pl->ps_val1;
		}
		return;
	
	case 60:	/* <operand> ::= <basereg> ( reg16 ) */
		curop.op_cls = (1L<<OCBX);
		if(p[1].ps_val1 == 0)
			err('E');
		curop.op_val = p[1].ps_val1;
		curop.op_rel = 0;
		curop.op_flg = pl->ps_val1;
		return;
	
	case 61:	/* <operand> ::= ccode */
		curop.op_cls = (1L<<OCCCODE);
		if(4 <= p->ps_val1 && p->ps_val1 < 8)
			curop.op_cls |= (1L<<OCCFLAG);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;
	
	case 62:	/* <operand> ::= int */
		curop.op_cls = (1L<<OCINT);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;
	
	case 63:	/* <operand> ::= ctl */
		curop.op_cls = (1L<<OCCTL);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;

	case 64:	/* <operand> ::= FLAGS */
		curop.op_cls = (1L<<OCFLAGS);
		curop.op_val = p->ps_val1;
		curop.op_rel = curop.op_flg = 0;
		return;

	case 65:	/* <basereg> ::= reg16 */
		if(segflg || p->ps_val1==0)
			err('E');
		return;
	
	case 66:	/* <basereg> ::= reg32 */
		if(!segflg || p->ps_val1==0)
			err('E');
		return;

	case 67:	/* <indexed> ::= <expr> ( reg16 ) */
		if(p[1].ps_val1 == 0)
			err('E');
		pl->ps_flg |= p[1].ps_val1;
		return;
	
	case 68:	/* <operand> ::= orop <expr> */
	case 69:	/* <operand> ::= orop <indexed> */
		curop.op_cls = (1L<<OCX);
		if(eflg) {
			curop.op_val = curop.op_rel = 0;
			curop.op_flg = OFSHORT;
		} else {
			curop.op_val = p->ps_val0;
			curop.op_rel = p->ps_val1;
			curop.op_flg = p->ps_flg | OFSHORT;
		}
		return;
	}
	return;
}
