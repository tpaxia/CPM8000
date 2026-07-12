#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"
#include "asz8k.h"

/*
 * Version 3.3, 8/27/82.  Changes to accomodate 8086 version.
 */
static	char	ident[] = "@(#)asz8kglo.c	3.3";

/*
 * Global variable definitions and initializations specific to asz8k.
 */
struct	chent	chtab[] = {	/* table of single-character tokens */
	{'~',TKUNOP},
	{'*',TKMULOP,TVMUL},
	{'/',TKMULOP,TVDIV},
	{'%',TKMULOP,TVMOD},
	{'+',TKADDOP,TVADD},
	{'-',TKADDOP,TVSUB},
	{'=',TKRELOP,TVEQ},
	{'&',TKANDOP},
	{'^',TKXOROP},
	{'|',TKOROP},
	{'(',TKLPAR},
	{')',TKRPAR},
	{'#',TKPOUND},
	{'@',TKAT},
	{',',TKCOM},
	{':',TKCOLON},
	{'\n',TKEOL},
	{-1,TKEOF},
	{0,TKERR}
};
int		extoff = 2;
uns		minaln = 1;
struct	operand	optab[OPMAX] = 0;
char		segflg = 0;
char		oflag = 0;
