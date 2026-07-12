/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "stdio.h"
#include "acom.h"
#include "obj.h"

/*
 * Version Z.3, 4/7/83.  Added explicit fclose of LIST and OBJECT files,
 *  in case of error exit.
 *
 * Version Z.2, 4/5/83.  Added external declarations of the variables
 *  segflg and oflag, which are used in chaining to xcon.
 *
 * Version Z.1, 10/14/82.  killed profiling and made cpm changes.
 *
 * Version 3.7, 10/13/82.  Changed dopass() so that the default section
 *  at the beginning of each pass is "__text" rather than the absolute
 * section.  This is a FUNCTIONAL CHANGE.
 *
 * Version 3.6, 8/20/82.  Changed laboc() to accomodate the colon
 * as a legitimate token rather than as a part of a label, for the
 * 8086 assembler.
 *
 * Version 3.5, 8/19/82.  Fixed two minor bugs:  1. Location counter
 * field sometimes came out too long on 32-bit machines.  2. Undefined
 * symbols sometimes went into the object file with non-zero values.
 *
 * Version 3.4, 7/5/82.  Added hooks for profiling.
 *
 * Version 3.3, 5/27/82.  Added uext (-u) switch.
 *
 * Version 3.2, 5/22/82.  Moved closing of files into special exit()
 * routine in a.misc.c.
 */
static	char	ident[] = "@(#)a.ascom.c	3.6";

extern	char	segflg;
extern	char	oflag;

char putfile[15];
char optfile[15];

main(argc,argv) int argc; char *argv[]; {

static	char	errfmt[] = "%u errors, %u warnings\n";
struct	octab	*ocp;
int	index;

	init(argc,argv);
	dopass();
	interlude();
	pass2 = 1;
	dopass();
	oflush();  objtyp = OBOND;  oflush();
	if(lflag) {
		pgcheck();  fputc('\n',LIST);
		pgcheck();  fprintf(LIST,errfmt,errct,warnct);
		if(xflag) putxref();
	}
	unlink("VM.TMP");
	if(errct) {
		fprintf(ERROR,errfmt,errct,warnct);
		fclose(LIST);
		fclose(OBJECT);
		exit(1);
	}
	argv[index = 0] = "xcon";
	index++;
	if (segflg)
		argv[index++] = "-s";
	if (oflag) {
		argv[index++] = "-o";
		argv[index++] = optfile;
	}
	argv[index++] = putfile;
	argv[index] = NULL;
	chain(argv);
}

/*
 * assem1 - Assembles a single statement.
 */
assem1() {

	laboc();
	if(*opcstr) {  /* we have an opcode field */
		opcode = oclook(opcstr);
		switch(opcode->oc_typ) {

		case OTUND:	/* undefined mnemonic */
			err('O');  skipeol();
			break;

		case OTINS:	/* machine instruction */
			instr(opcode->oc_val);
			break;

		case OTDIR:	/* assembler directive */
			direc(opcode->oc_val);
			break;

		case OTMAC:	/* macro call */
			macro(opcode->oc_val);
			break;

		}
	} else if(*labstr) {  /* we have a label field standing alone */
		label = sylook(labstr);
		assign(STLAB,curloc,cursec);
	}
}

/*
 * assign - If a label is present, this routine gives it the specified type,
 * value, and relocation, checking for multiple definition and phase errors.
 * It also records a cross reference entry.  The returned value is a pointer
 * to the label's sytab entry (in physical memory), or 0 if there was no
 * label or there was an error.
 */
struct sytab *
assign(typ,val,rel) uns typ; exprval val; uns rel; {

struct	sytab	*syp;

	sprintf(llloc,"%04x",(uns)(val&0xffff));
	if(label == 0) return(0);  /* no label */
	xref(label,XRDEF);
	syp = wfetch(label);
	if(syp->sy_typ!=STVAR&&(pass2?syp->sy_atr&SADP2:syp->sy_typ!=STUND)) {
		err('M');  syp->sy_atr |= SAMUD;
		return(0);
	}
	if(syp->sy_typ == STUND) {  /* assign a type */
		syp->sy_typ = typ;  syp->sy_rel = RBUND;
	} else if(syp->sy_typ != typ) goto phaser;
	if(syp->sy_rel==RBUND || syp->sy_typ==STVAR) {  /* assign a value */
		syp->sy_val = val;  syp->sy_rel = rel;
	} else if(syp->sy_val!=val || syp->sy_rel!=rel) goto phaser;
	if(pass2) syp->sy_atr |= SADP2;
	return(syp);
phaser:
	err('P');
	return(0);
}

/*
 * def1 - Copies one statement into a macro definition.
 */
def1() {

int	argno;

	/*
	 * Check for an ADMAC or ADENDM directive.
	 */
	laboc();
	if(*opcstr) {  /* opcode field */
		opcode = oclook(opcstr);
		if(opcode->oc_typ == OTDIR) {  /* directive */
			if(opcode->oc_val == ADMAC)  {  /* nested macro def */
				deflev++;
			} else if(opcode->oc_val == ADENDM)  {  /* end def */
				deflev--;
			}
		}
	}
	if(deflev > 0) {  /* copy a line of the definition */
		scanpt = sline;  /* reset scan to beginning of line */
		do {
			scanc();
			if(ch == escchr) {  /* escaped character */
				scanc();
				*wfetch(valloc(1)) = ch;
			} else if(ch == argchr) {  /* macro parameter */
				scanc();
				if(ch == mctchr)  /* macro expansion ct */
					*wfetch(valloc(1)) = 0200;
				else if(ch == argchr)  /* extra args */
					*wfetch(valloc(1)) = 0201;
				else if('0'<=ch && ch<='9') {
					argno = ch-'0';
					if(argno > curdef->oc_arg)
						curdef->oc_arg = argno;
					*wfetch(valloc(1)) = argno+0202;
				} else err('A');
			} else {  /* normal character */
				*wfetch(valloc(1)) = ch;
			}
		} while(ch != '\n');
		unscanc();  token();
	} else {  /* finish off the definition */
		*wfetch(valloc(1)) = '\0';
	}
	skipeol();
}

/*
 * delim - Skips over a delimiter string if one is present.
 */
delim() {

	if(toktyp == TKSPC) iilex();
	if(toktyp == TKCOM) iilex();
	if(toktyp == TKSPC) iilex();
}

/*
 * dopass - Performs one pass of the source input.
 */
dopass() {

	if(include(srcfile) != 0) {
		fprintf(ERROR,"Cannot open %s\n",srcfile);
		exit(1);
	}
	condlev = curatr = curloc = cursec = deflev = mexct =
	 rptlev = sectab[0].se_loc = truelev = 0;
	curaln = minaln;
	curext = 32;
	reading = secct = 1;
	label = sylook("__text");  newsec();	/* set the default section */
	while(reading) {  /* process statements one at a time */
		if(deflev > 0) {  /* defining a macro */
			def1();
		} else if(rptlev > 0) {  /* defining a repeat */
			rpt1();
		} else if(condlev > truelev) {  /* skipping conditional code */
			skip1();
		} else {  /* assembling statements normally */
			assem1();
		}
		if(toktyp == TKEOF)  break;
		if(toktyp != TKEOL) {
			err('S');  skipeol();
		}
		putline();
	}
	setsec(0);  /* update sectab info for last section */
	while(toktyp != TKEOF) token();
}

/*
 * emitb - Emits a byte of object code.
 */
emitb(value,reloc) uns value, reloc; {

	if(pass2) {
		/*
		 * Check whether relocation is needed.
		 */
		if((reloc&RAMSK)==RANOP || (reloc&RBMSK)==RBABS) reloc = 0;
		/*
		 * Output to the object file without breaking up a relocatable
		 * item.
		 */
		if(relbot-objtop<(reloc?7:1)) oflush();
		setorg();
		if(reloc) {
			*--relbot = reloc>>8;
			*--relbot = reloc;
			*--relbot = objtop-objbuf;
		}
		oputb(value);  objbuf[5]++;
		/*
		 * Output to the listing.
		 */
		if(llobt >= llobj+LLOBJ) {
			putline();
			sprintf(llloc,"%04x",(uns)curloc);
		}
		sprintf(llobt,"%02x",value&0377);
		llobt = llobt + 2;
	}
	nxtloc = ++curloc;
}

/*
 * emitl - Emits a long to the object file.
 */
emitl(value,reloc) long value; uns reloc; {

	emitw((uns)value,reloc);
	emitw((uns)(value>>16),0);
}

/*
 * emitw - Emits a word to the object file.
 */
emitw(value,reloc) uns value, reloc; {

	emitb(value,reloc);
	emitb(value>>8,0);
}

/*
 * err - Puts the specified error flag into the listing.
 */
err(c) int c; {

	if(!pass2 || llert>=llerr+LLERR) return;
	if(c<=' ' || c>='\177') c = '?';
	*llert++ = c;
	*llert = '\0';
	errct++;
}

/*
 * expression - Parses an expression and checks for various error conditions.
 * the expression's value and relocation are left in curop.  At call time,
 * iilexeme should contain the first token of the expression.
 */
expression() {

	if(iiparse() != 0) {  /* error */
		curop.op_cls = 0L;
		while(toktyp!=TKCOM && toktyp!=TKEOL) token();
	}
	if(!(curop.op_cls&1L<<OCEXP)) {  /* not expression */
		err('S');  curop.op_val = 0;  curop.op_rel = RBUND;
	}
}

/*
 * interlude - Performs processing between pass 1 and pass 2.
 */
interlude() {

struct	sytab	*syp;
vmadr	p;
uns	h;
uns	rel;
char	type;

	objtyp = OBOST;  oflush();
	for(rel=RBSEC ; rel<secct ; rel++) {  /* output sections blocks */
		if(objtyp!=OBSEC || relbot-objtop<SYMSIZ+7) {
			oflush();  objtyp = OBSEC;
		}
		syp = (struct sytab *)rfetch(sectab[rel].se_sym);
		oputb(sectab[rel].se_aln);
		oputb(sectab[rel].se_ext);
		oputb(sectab[rel].se_atr);
		oputs(syp->sy_str);
	}
	rel = RBEXT;
	for(h=0 ; h<1<<SHSHLOG ; h++) {
		for(p=syhtab[h] ; p ; p=syp->sy_lnk) {
			syp = (struct sytab *)rfetch(p);
			if(syp->sy_typ==STKEY || syp->sy_typ==STSEC) continue;
			if(syp->sy_atr&SAGLO || uext&&syp->sy_typ==STUND)
				type = OBGLO;
			else
				type = OBLOC;
			if(objtyp!=type || relbot-objtop<SYMSIZ+7) {
				oflush();  objtyp = type;
			}
			if(syp->sy_typ == STUND) {
				oputl(0L);
				oputb(RBUND);
				if(syp->sy_atr&SAGLO || uext) {
					if(rel >= RBMSK) {
						fprintf(ERROR,"Too many externals\n");
						exit(1);
					}
					syp = (struct sytab *)wfetch(p);
					syp->sy_typ = STLAB;
					syp->sy_atr |= SAGLO;
					syp->sy_val = 0;
					syp->sy_rel = rel++;
				}
			} else {
				oputl((long)syp->sy_val);
				oputb(syp->sy_rel);
			}
			oputs(syp->sy_str);
		}
	}
}

/*
 * laboc - Scans the label and opcode fields from the next assembler
 * statement, and leaves their strings in global arrays labstr and
 * opcstr.  Each string is set to null if its field is missing.  Toktyp
 * is left on the token after the opcode (if it is present).
 */
laboc() {

	labstr[0] = opcstr[0] = 0;
	if(token() == TKSYM) {  /* label in column 1, possibly with colon */
		symcpy(labstr,tokstr);  labstr[SYMSIZ] = '\0';
		if(token() == TKCOLON)  /* colon label */
			token();
		if(toktyp == TKSPC) token();
		if(toktyp == TKSYM) {  /* opcode */
			symcpy(opcstr,tokstr);  opcstr[SYMSIZ] = '\0';
			token();
		}
	} else if(toktyp==TKSPC && token()==TKSYM) { /* colon label or opcode */
		symcpy(labstr,tokstr);  labstr[SYMSIZ] = '\0';
		if(token() == TKCOLON) {  /* was a label, check for opcode */
			if(token() == TKSPC) token();
			if(toktyp == TKSYM) {  /* opcode */
				symcpy(opcstr,tokstr);  opcstr[SYMSIZ] = '\0';
				token();
			}
		} else {  /* oops, was opcode, not label */
			symcpy(opcstr,labstr);
			opcstr[SYMSIZ] = labstr[0] = '\0';
		}
	}
}

/*
 * macro - Processes a macro call.
 */
macro(vp) vmadr vp; {

struct	input	*newfp;
int	brlev;
int	i;
char	**avp;
char	*sp;
char	mch;
char	nbuf[6];

	/*
	 * Create an input stack frame with room for argument pointers
	 * at the beginning of the variable area.
	 */
	newfp = pushin();  avp = insp;
	insp += (opcode->oc_arg+3)*sizeof(char *);  iovck();
	newfp->in_typ = INMAC;
	/*
	 * Push the macro expansion count (?#) string, and initialize the
	 * extra arguments (??) string to null.
	 */
	avp[0] = insp;
	sprintf(nbuf,"%u",++mexct);
	sp = nbuf;
	while(*sp != '\0') pushc(*sp++);
	avp[1] = insp;
	pushc('\0');
	/*
	 * Push the label (?0) string.
	 */
	avp[2] = insp;
	sp = labstr;
	i = SYMSIZ;
	while(--i>=0 && *sp!='\0') pushc(*sp++);
	pushc('\0');
	/*
	 * Push the operand field arguments.
	 */
	if(toktyp == TKSPC) scanc();
	for(i=1 ; i<=opcode->oc_arg||ch!='\n'&&ch!=';' ; i++) {
		if(i <= opcode->oc_arg) {  /* an expected argument */
			avp[i+2] = insp;
		} else {  /* an extra argument */
			if(i == opcode->oc_arg+1) {  /* first extra arg */
				avp[1] = insp;
			} else {  /* additional extra arg */
				insp[-1] = ',';
			}
			pushc(lbrchr);
		}
		if(ch == lbrchr) {  /* argument enclosed in braces */
			brlev = 1;
			for(;;) {
				scanc();
				if(ch == lbrchr) brlev++;
				else if(ch == rbrchr) brlev--;
				if(brlev<=0 || ch=='\n') break;
				if(ch == escchr) scanc();
				pushc(ch);
			}
			if(ch == rbrchr) scanc();
		} else {  /* normal argument (not in braces) */
			while(ch!=',' && !white(ch) && ch!='\n' && ch!=';') {
				pushc(ch);  scanc();
			}
		}
		if(i > opcode->oc_arg) pushc(rbrchr);
		pushc('\0');
		while(white(ch)) scanc();
		if(ch == ',') scanc();
		while(white(ch)) scanc();
	}
	while(ch != '\n') scanc();
	toktyp = TKEOL;
	/*
	 * Copy the macro body onto the input stack.
	 */
	newfp->in_ptr = insp;
	while(mch = *rfetch(vp++)) {
		pushc(mch);  newfp->in_cnt++;
	}
	/*
	 * Switch input source to macro stack frame.
	 */
	infp = newfp;
	if((curlst = infp->in_lst&0377) != 0) curlst--;
}

/*
 * newsec - Creates a new section and makes it the current section.
 */
newsec() {

	if(secct >= SECSIZ) {
		fprintf(ERROR,"Too many sections\n");
		exit(1);
	}
	assign(STSEC,(exprval)0,secct);
	sectab[secct].se_sym = label;
	setsec(secct++);
	curatr = curloc = 0;
	curaln = minaln;
	curext = 32;
}

/*
 * oflush - Outputs an object block to the object file.
 */
oflush() {

char	*op;

	if(objtyp) {
		fputc(objtyp,OBJECT);
		fputc(OBJSIZ-(relbot-objtop),OBJECT);
		for(op=objbuf ; op<objtop ; op++) fputc(*op,OBJECT);
		for(op=relbot ; op<&objbuf[OBJSIZ] ; op++) fputc(*op,OBJECT);
	}
	objtop = objbuf;  relbot = &objbuf[OBJSIZ];  objtyp = 0;
}

/*
 * oputb - Puts a byte into the object buffer.
 */
oputb(c) char c; {

	if(objtop >= relbot) {
		fprintf(ERROR,"Object buffer overflow\n");
		exit(1);
	}
	*objtop++ = c;
}

/*
 * oputl - Puts a long word into the object buffer.
 */
oputl(l) long l; {

	oputw((uns)l);
	oputw((uns)(l>>16));
}

/*
 * oputs - Puts a symbol into the object buffer.
 */
oputs(s) char *s; {

char	i;

	i = SYMSIZ;
	do {
		if(*s == '\0') break;
		oputb(*s++);
	} while(--i > 0);
	oputb('\0');
}

/*
 * oputw - Puts a word into the object buffer.
 */
oputw(w) uns w; {

	oputb(w);
	oputb(w>>8);
}

/*
 * rpt1 - Copies one statement into a repeat definition.
 */
rpt1() {

struct	input	*newfp;
vmadr	vp;
char	rch;

	laboc();
	if(*opcstr) {  /* check opcode field for special cases */
		opcode = oclook(opcstr);
		if(opcode->oc_typ == OTDIR) {  /* directive */
			if(opcode->oc_val == ADREPT) {  /* nested repeat */
				rptlev++;
			} else if(opcode->oc_val == ADENDR) {  /* end rpt */
				rptlev--;
			}
		}
	}
	if(rptlev > 0) {  /* copy a line of the repeat definition */
		scanpt = sline;  /* reset scan to beginning of line */
		do {  /* copy the line */
			scanc();
			*wfetch(valloc(1)) = ch;
		} while(ch != '\n');
		unscanc();  token();
	} else {  /* finish off the repeat definition and start the repeat */
		*wfetch(valloc(1)) = '\0';
		newfp = pushin();
		newfp->in_typ = INRPT;
		newfp->in_rpt = rptct;
		vp = rptstr;
		while(rch = *rfetch(vp++)) {  /* copy definition to stack */
			pushc(rch);
		}
		newfp->in_ptr = insp;  /* set up as though at end of frame */
		virtop = rptstr;  /* free up the virtual space */
		infp = newfp;  /* switch to new input stack frame */
		if((curlst = infp->in_lst&0377) != 0) curlst--;
	}
	skipeol();
}

/*
 * setorg - Sets up the next address for text output.
 */
setorg() {

	if(objtyp!=OBTXT || nxtloc!=curloc || nxtsec!=cursec) {
		oflush();  objtyp = OBTXT;
		oputl((long)curloc);  oputb(cursec);  oputb(0);
		nxtloc = curloc;  nxtsec = cursec;
	}
}

/*
 * setsec - Changes to the specified section for code generation.
 */
setsec(sec) uns sec; {

	sectab[cursec].se_aln = curaln;
	sectab[cursec].se_ext = curext;
	sectab[cursec].se_atr = curatr;
	sectab[cursec].se_loc = curloc;
	cursec = sec;
	curaln = sectab[cursec].se_aln;
	curext = sectab[cursec].se_ext;
	curatr = sectab[cursec].se_atr;
	curloc = sectab[cursec].se_loc;
}

/*
 * skip1 - Skips one statement due to an unsatisfied conditional assembly.
 */
skip1() {

	laboc();
	opcode = *opcstr?oclook(opcstr):0;
	if(opcode && opcode->oc_typ==OTDIR) {
		switch(opcode->oc_val) {

		case ADIF:	/* .if */
			condlev++;
			break;

		case ADELSE:	/* .else */
			if(truelev == condlev-1) truelev++;
			break;

		case ADENDIF:	/* .endif */
			condlev--;
			break;

		}
	}
	if(condlev > truelev) {  /* mark listing line as skipped */
		sprintf(llobj,"%8s","-");	/* 8 should be LLOBJ */
		llobt = llobj + LLOBJ;
	} else
		/*
		 * End of the skip.  List the directive regardless of
		 * the condlst value.
		 */
		llfull = curlst;
	skipeol();
}

/*
 * skipeol - Skips to the next end of line or end of file.
 */
skipeol() {

	while(toktyp!=TKEOL && toktyp!=TKEOF) token();
}

/*
 * title - Puts a title into the specified string, and fixes up the listing.
 */
title(s) char *s; {

	if(!pass2) return;
	if(toktyp==TKSPC && token()==TKSTR) {
		tokstr[TITSIZ] = '\0';
		strcpy(s,tokstr);
		token();
		llfull = linect = 0;
	} else err('S');
}

/*
 * warn - Puts the specified warning flag into the listing.
 */
warn(c) int c; {

	if(!pass2 || llert>=llerr+LLERR) return;
	if(c<=' ' || c>='\177') c = '?';
	*llert++ = c;
	*llert = '\0';
	warnct++;
}

