#include "portab.h"
#include "bdos.h"
/*
 * chain - format argv into a CCP command line and chain to it (BDOS 47).
 *
 * The assembler (aascom.c) relies on program termination to flush and close
 * its open files -- it does not fclose(OBJECT) on the success path.  So this
 * wrapper MUST call _cleanup() (what exit() uses to flush+close every open
 * stream) before invoking BDOS 47; otherwise the tail of the .OBJ file is
 * left unflushed and xcon reports "Premature EOF".
 *
 * Per the Chain To Program function, the command line is placed in the
 * current DMA buffer as <count><chars><null>.  Does not return.
 */
chain(argv)
char **argv;
{
	static char _cmdbuf[132];
	char *p, *s;
	int i;

	p = &_cmdbuf[1];
	for (i = 0; argv[i] != 0; i++) {
		if (i != 0) *p++ = ' ';
		for (s = argv[i]; *s != 0; ) *p++ = *s++;
	}
	*p = 0;
	_cmdbuf[0] = p - &_cmdbuf[1];
	_cleanup();               /* flush & close all open files (assembler depends on this) */
	_setdma((long) _cmdbuf);
	_chain();
}
