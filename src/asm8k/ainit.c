#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

/*
 * Version Z.1, 4/5/83.  Added external declarations of segflg and oflag.
 */

/*
 * Version 3.2, 5/27/82.  Added uext (-u) switch.
 */
static	char	ident[] = "@(#)a.init.c	3.2";

extern	char	segflg;
extern	char	oflag;

/*
 * badpre - Issues a fatal error for a bad PREDEF file.
 */
badpre() {

	fprintf(ERROR,"PREDEF file error at line %u\n",infp->in_seq);
	exit(1);
}

/*
 * getdat - Gets the date and time and puts them into datstr.
 *	    Disabled in CP/M version.
 */
getdat() {
}
#define BinFile	1
#define AscFile	0

extern	char	putfile[];
extern	char	optfile[];
/*
 * init - Performs assembler initialization.
 */
init(argc,argv) int argc; char *argv[]; {

char	**av;
char	*ap;
char	*ep;
char	*sp;
char	fname[15];
int	fd;
FILE	*newfile();

	phytop = phylim = sbrk(0);  /* needed only for monitoring */
	getdat();
	prname = "asz8k";
	while((ap = *++argv) && ap!=-1) {  /* read command line arguments */
		if(*ap == '-') {  /* switches */
			while(*++ap) switch(*ap) {

			case 'l':
				lflag = 1;
				break;

			case 'o':
				oflag = 1;
				if ((**argv == '\0') || (*argv == NULL))
					{printf("called from case 'o'\n");
					 usage();}
				strcpy(optfile,*++argv);
				break;

			case 'p':
				pflag = 1;
				break;

			case 's':
				segflg = 1;
				break;

			case 'u':
				uext = 1;
				break;

			case 'x':
				xflag = lflag = 1;
				break;

			default:
				printf("error case\n");
				usage();

			}
		} else {  /* file name */
			if(srcfile) {printf("srcfile != 0\n"); usage();}
			srcfile = ap;
		}
	}
	if(!srcfile) {printf("no srcfile\n"); usage();}
	if((sp = rindex(srcfile,'/')) == 0) sp = srcfile; else sp++;
	if(strlen(sp) > 14) sp[14] = '\0';
	strcpy(titl1,srcfile);
	strcpy(fname,sp);
	if((ep = rindex(fname,'.')) == 0)
		usage();
	else
		ep++;
	if( segflg ) {		/* Prog name must end with "8ks". */
	    if( strcmp (ep,"8ks") != 0) {
		printf("Segmented source file must end with '.8ks'\n");
		usage();
	    }
	}
	else 			/* Prog name must end in "8kn". */
	    if( strcmp (ep,"8kn") != 0) {
		printf("Nonsegmented source file must end with '.8kn'\n");
		usage();
	    }
	if((fd = opena(srcfile,0)) == -1) {
		fprintf(ERROR,"Cannot open %s\n",srcfile);
		exit(1);
	}
	strcpy(ep,"obj");  OBJECT = newfile(fname,BinFile);
	strcpy(putfile,fname);
	if(lflag) {
		strcpy(ep,"lst");  LIST = newfile(fname,AscFile);
	}
	vinit();
	predef();
}

/*
 * newfile - Creates a new file of the specified name,
 * and returns the file descriptor.
 */
FILE *
newfile(s,asci) char *s; char asci; {

FILE	*fd;

	if((fd = _fopen(s,"w",asci)) == NULL) {
		fprintf(ERROR,"Cannot create %s\n",s);
		exit(1);
	}
	return(fd);
}

/*
 * preget - Gets a token, issues a fatal error if it is not the specified
 * type, and returns the token's value.
 */
preget(typ) int typ; {

	if(token() != typ) badpre();
	return(tokval);
}

/*
 * usage - Issues a fatal error for an illegal command line.
 */
usage() {

	fprintf(ERROR,"Usage:  asz8k [-o outfile] [-luxs] file.8k{n|s}\n");
	exit(1);
}

