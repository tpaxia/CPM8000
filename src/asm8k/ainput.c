#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

/*
 * Version 3.2, 7/5/82.  Performance enhancements.
 */

static	char	ident[] = "@(#)a.input.c	3.2";

/*
 * fillin - Processes the end of an input stack frame.
 */
fillin() {

reg	struct	input	*rinfp;

	switch((rinfp = infp)->in_typ) {

	case INFILE:
		rinfp->in_ptr = rinfp->in_buf;
		rinfp->in_cnt = read(rinfp->in_fd,rinfp->in_buf,
		  1<<BLKLOG);
		if(rinfp->in_cnt == 0) {
			close(rinfp->in_fd);
			popin();
		}
		return;

	case INARG:
	case INMAC:
		popin();
		return;

	case INRPT:
		if(rinfp->in_rpt-- != 0) {
			rinfp->in_cnt = rinfp->in_ptr-rinfp->in_buf;
			rinfp->in_ptr = rinfp->in_buf;
			rinfp->in_seq = 0;
		} else popin();
		return;

	}
}

/*
 * include - Pushes the specified file into the input stream.  Returns -1
 * if the file cannot be opened, 0 otherwise.
 */
include(file) char *file; {

int	fd;

	if((fd = opena(file,0)) == -1) return(-1);
	infp = pushin();
	insp += 1<<BLKLOG;  iovck();
	infp->in_typ = INFILE;
	infp->in_fd = fd;
	if((curlst = infp->in_lst&0377) != 0) curlst--;
	return(0);
}

/*
 * iovck - Checks to make sure the input stack pointer is not beyond the
 * end of the input stack area.
 */
iovck() {

	if(insp > instk+INSIZ) {
		fprintf(ERROR,"Input stack overflow\n");
		exit(1);
	}
}

/*
 * macarg - Pushes a macro argument frame on the input stack.  The parameter
 * is the argument designator.
 */
macarg(argno) int argno; {

reg	struct	input	*rinfp;
char	*ap;
char	**avp;

	avp = infp->in_buf;
	ap = avp[argno&0177];
	rinfp = infp = pushin();
	rinfp->in_typ = INARG;
	rinfp->in_ptr = ap;
	rinfp->in_cnt = strlen(ap);
	if((curlst = rinfp->in_lst&0377) != 0) curlst--;
}

/*
 * popin - Pops the top frame off the input stack.
 */
popin() {

	curlst = infp->in_lst&0377;  insp = infp;  infp = infp->in_ofp;
}

/*
 * pushc - Pushes the specified character onto the input stack.
 */
pushc(c) char c; {

char	*oldsp;

	oldsp = insp++;  iovck();
	*oldsp = c;
}

/*
 * pushin - Pushes a new frame onto the input stack.  The buffer area in_buf
 * is not allocated, since it is of variable size.  The value returned is
 * the new frame pointer.  Note that the global frame pointer infp is
 * not automatically adjusted.
 */
struct input *
pushin() {

struct	input	*newfp;

	if((int)insp&01) insp++;  /* force integer alignment */
	newfp = insp;
	insp = &newfp->in_buf[0];  iovck();
	newfp->in_ofp = infp;
	newfp->in_seq = newfp->in_cnt = 0;
	newfp->in_lst = curlst;
	return(newfp);
}
