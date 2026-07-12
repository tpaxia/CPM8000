#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

/*
 * Version 3.3, 8/20/82.  Made colon a real token and got rid of the TKLAB
 * token type, to accomodate 8086 version.
 *
 * Version 3.2, 7/5/82.  Performance enhancements.
 */

static	char	ident[] = "@(#)a.token.c	3.3";

/*
 * stresc - Processes a string escape.  This routine is called after the
 * escape character (normally '\') has been seen.  It scans off the remainder
 * of the escape and returns its value.
 */
stresc() {

int	ct,
	val;

	scanc();
	if('0'<=ch && ch<='7') {  /* an octal escape */
		val = 0;  ct = 3;
		do {
			val = (val<<3)+ch-'0';
			scanc();
		} while(--ct>0 && '0'<=ch && ch<='7');
		unscanc();
		return(val);
	}
	switch(ch) {  /* a single-character escape */

	case 'b':
	case 'B':
		return('\b');

	case 'f':
	case 'F':
		return('\f');

	case 'n':
	case 'N':
		return('\n');

	case 'r':
	case 'R':
		return('\r');

	case 't':
	case 'T':
		return('\t');

	default:
		return(ch);

	}
}

/*
 * token - Scans the next token and returns its type.  Also sets up the
 * token's type and value information in the global cells toktyp, tokval,
 * and tokstr.
 */
token() {

int	c;
struct	chent	*chp;
char	radix;
char	*strp;

	strp = tokstr;
	scanc();
#ifdef DEBUG
	printf("%c(%o)",ch,ch);
#endif
	if(white(ch)) {  /* white space */
		do {
			scanc();
		} while(white(ch));
		if(ch!=';' && ch!='\n') {
			unscanc();
			return(toktyp = TKSPC);
		}
	}
	if(ch == ';') {  /* comment */
		do {
			scanc();
		} while(ch != '\n');
		return(toktyp = TKEOL);
	}
	if('a'<=ch&&ch<='z' || 'A'<=ch&&ch<='Z' || ch=='$' || ch=='_'
	 || ch=='.') {  /* symbol */
		do {
			if(strp < tokstr+SYMSIZ) *strp++ = ch;
			scanc();
		} while('a'<=ch&&ch<='z' || 'A'<=ch&&ch<='Z' ||
		 '0'<=ch&&ch<='9' || ch=='$' || ch=='_' || ch=='.');
		*strp = '\0';
		unscanc();
		return(toktyp = TKSYM);
	}
	if('0'<=ch && ch<='9') {  /* constant */
		do {
			*strp++ = ch;
			scanc();
			if('A'<=ch && ch<='Z') ch += 'a'-'A';
		} while('0'<=ch&&ch<='9' || 'a'<=ch&&ch<='z');
		unscanc();
		*strp = '\0';
		c = *--strp;
		if('0'<=c && c<='9') radix = 10;
		else {
			switch(c) {

			case 'b':
				radix = 2;
				break;

			case 'd':
				radix = 10;
				break;

			case 'h':
				radix = 16;
				break;

			case 'o':
			case 'q':
				radix = 8;
				break;

			default:
				return(toktyp = TKERR);
			}
			*strp = '\0';
		}
		tokval = 0;  strp = tokstr;
		while(c = *strp++) {
			if('0'<=c && c<='9') c -= '0';
			else c += 10-'a';
			if(c >= radix) return(toktyp = TKERR);
			tokval = tokval*radix+c;
		}
		return(toktyp = TKCON);
	}
	if(ch == '\'') {  /* character constant */
		scanc();
		if(ch == '\n') {
			unscanc();
			return(toktyp = TKERR);
		}
		tokval = (ch==escchr)?stresc():ch;
		scanc();
		if(ch != '\'') {
			unscanc();
			return(toktyp = TKERR);
		}
		return(toktyp = TKCON);
	}
	if(ch == '"') {  /* quoted string */
		scanc();
		while(ch != '"') {
			if(ch == '\n') {
				unscanc();
				return(toktyp = TKERR);
			}
			if(strp < tokstr+STRSIZ) {
				*strp++ = (ch==escchr)?stresc():ch;
			}
			scanc();
		}
		*strp = '\0';
		tokval = strp-tokstr;
		return(toktyp = TKSTR);
	}
	if(ch == '<') {
		scanc();
		if(ch == '<') {
			tokval = TVSHL;
			return(toktyp = TKMULOP);
		}
		if(ch == '=') {
			tokval = TVLE;
			return(toktyp = TKRELOP);
		}
		unscanc();
		tokval = TVLT;
		return(toktyp = TKRELOP);
	}
	if(ch == '>') {
		scanc();
		if(ch == '>') {
			tokval = TVSHR;
			return(toktyp = TKMULOP);
		}
		if(ch == '=') {
			tokval = TVGE;
			return(toktyp = TKRELOP);
		}
		unscanc();
		tokval = TVGT;
		return(toktyp = TKRELOP);
	}
	if(ch == '!') {
		scanc();
		if(ch == '=') {
			tokval = TVNE;
			return(toktyp = TKRELOP);
		}
		unscanc();
		return(toktyp = TKERR);
	}
	/* single character token */
	for(chp=chtab ; chp->ch_chr ; chp++) {
		if(ch == chp->ch_chr) break;
	}
	tokval = chp->ch_val;
	return(toktyp = chp->ch_typ);
}

/*
 * xscanc - Takes care of the cases of character scanning which would be
 * difficult for the macro scanc() to handle.  Namely, these are end of line
 * processing and continuation processing.
 */
xscanc() {

top:
	if(ch == '\0') {
		getline();
		ch = *scanpt++;
		goto top;
	}
	if(ch==escchr && *scanpt=='\n') {  /* continuation */
		scanpt++;
		putline();
		ch = *scanpt++;
		goto top;
	}
	return(ch);
}
