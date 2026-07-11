#include "stdio.h"
#include "xout.h"

#define uns unsigned
#define reg register

/* this is the Z8000 loader -- it uses as input and output
   x.out format modules -- see ld8k.man for details */

#define MODNO 256	/* max allowable modules per load */
#define MAPNO 512	/* max map entries per load */
#define SYMPNO 4096	/* total number of symbols in all modules */
#define SEGNO 128	/* max number of segments per module */
#define FILNO 128	/* max number of files to process */
#define POSNO 256	/* max number of items in libraries */
#define SYMNO 2048	/* max number of symbols (total!) */
#define SEGHASH 128	/* max number of segment names */
#define SYMHASH 1024	/* max number of external symbols */
#define X_SY_IND 5	/* indirection in symbol table, use val */


#define AR8KMAGIC 0177545
#define AR8SIZ sizeof( ar8k_hd )
#define XHSIZ sizeof( x_hdr )
#define RLC 64
#define RSIZ (RLC * sizeof( struct x_rel ))

#define islower(c) ((c)>= 'a' && (c)<= 'z')
#define isupper(c) ((c)>= 'A' && (c)<= 'Z')
#define isalpha(c) ( islower(c) || isupper(c) )
#define isdigit(n) ((n)>= '0' && (n)<= '9')
#define oktyp(old,new) (ty[((old)<<3)+(new)] & OK)
#define newtyp(old,new) (ty[((old)<<3)+(new)] & 07)
#define OK 0200
#define X_SG_UND 0

#define round(x) if((x)&1)(x)++


/* now we define the data structures we will use */

/* the following structure is the place where we read the head of
   an archive group,  this tells us all we want to know about the
   file immediately following it */

struct ar8k_hd {
	char	ar8k_name[14];
	long	ar8k_date;
	char	ar8k_uid,	/* not used except for compatibility */
		ar8k_gid;	/* only for compatibility */
	int	ar8k_mode;	/* compatibility */
	long	ar8k_size;
}
	ar8k_hd;		/* header from the old archive */






/* the next series of items provides a way of going between the
   segment numbers as they appear in input modules, and the segment
   numbers as they must appear in output modules.  This map is kept
   on a module by module basis.  As we process each module in pass1,
   we make entries in this table, and we refer to it again in the pass2
   processing.  This is done by tracking the current module being
   processed during each pass in the variable "modno".  Thus is
   behooves us to process the modules in exactly the same order during
   both passes.  In particular, if the i'th modules has n segments,
   then there will be n entries in the character array "map",
   ranging from modmap[i].mapndx to modmap[i].mapndx+n-1.
   The same approach is used to map the symbol table numbers from
   the original module numbers to the final table numbers  */

short	modmap[MODNO],		/* the mapping (by module) of segment numbers */
	modsym[MODNO],		/* the mapping (by module) of symbol numbers */
	modno;		/* number of current module being processed */

char	map[MAPNO];	/* list of internal to external segment numbers */
short	symap[SYMPNO];	/* the list of internal to external symbol numbers */

short	mapx,		/* index of next entry in map table */
	maps,		/* starting positon of map, this entry */
	symx,		/* index of next entry in symap table */
	syms;		/* starting position of symap, this entry */


/* the following data structures keeps information about each segment
   that we are building.  This must, naturally, contain all of the information
   used in preparing the segment description portion of the output file
   (which is actually the last thing written).  During pass 1 processing
   we keep track of how big each segment currently is so that we can
   decide which (output) segment to put the current (input) segment.
   Segments can never be bigger than 65536 bytes by virtue of the way
   the Z8001 works.  During pass 2 processing the member "segpos" is
   used to track where we are writing into the output file */

struct segtab {
	long	segsiz;		/* size of this segment (built in pass1) */
	long	segpos;		/* file position to write in (pass2) */
	uns	hiwat;		/* size of segment at end of pass1 */
	uns	segrel;		/* relocation bias during pass2 */
	char	segtyp;		/* type of this segment */
	char	segalloc;	/* non-zero if pre allocated */
}
	segtab[SEGNO];


short	seghash[SEGHASH];	/* this table is used as a hash entry table
				   for the symbol table indexes for entries
				   that are segment names */

short	symhash[SYMHASH];	/* this table is used as a hash entry table
				   for symbol table indexes for symbols that
				   are external names */

/* the x.out header structures follow

   there is one structure for the header, x_hdr.  It is used in reading
   each of the input files, and for creating the output file.  Then there
   is an array of segment header structures, x_sg[].  These are also used
   in reading the input and for creating the output
*/

struct x_hdr	x_hdr;		/* the x.out header structure */

struct x_sg	x_sg[SEGNO];	/* the x.out segment structures */


/* the next structure is used to control the processing of files that
   are single modules and those that are libraries.  If the file is
   a library, we record during pass1 those modules that it contains
   that should be loaded.  This is done by means of a list of lseek
   positions to the start of each module to load.  This list is teminated
   by a -1  */

struct filtab {
	char	*filname;	/* the actual string for the file name */
	short	posndx;		/* first (of several) entries in postab
				   -1 == not a library
				   0  == (pass1) lib
				   0  == (pass2) lib, but no files to load */
}
	filtab[FILNO];
short	filndx;			/* index of next entry in filtab */

long	postab[POSNO];		/* seek positions in files */
short	posmax;			/* next entry position in postab */


/* the main symbol table */

struct x_sym symtab[SYMNO];

short	symlev = {1},		/* next free slot in symtab (0 unused) */
	symtop;			/* current end of read in symbol table */

struct x_rel	rlibuf[RLC],	/* input relocation read here */
		rlobuf[RLC];	/* output relocation put here */

/* the type clash and mixing table */

char	ty[] = {
	OK+X_SG_UND, OK+X_SG_BSS, OK+X_SG_STK, OK+X_SG_COD,
	OK+X_SG_CON, OK+X_SG_DAT, OK+X_SG_MXU, OK+X_SG_MXP,
	OK+X_SG_BSS, OK+X_SG_BSS,    X_SG_BSS,    X_SG_MXU,
	   X_SG_DAT,    X_SG_DAT, OK+X_SG_MXU,    X_SG_MXU,
	OK+X_SG_STK,    X_SG_BSS, OK+X_SG_STK,    X_SG_MXU,
	   X_SG_MXU,    X_SG_DAT, OK+X_SG_MXU,    X_SG_MXU,
	OK+X_SG_COD,    X_SG_MXU,    X_SG_MXU, OK+X_SG_COD,
	   X_SG_MXP,    X_SG_MXU,    X_SG_MXU, OK+X_SG_MXP,
	OK+X_SG_CON,    X_SG_DAT,    X_SG_DAT,    X_SG_MXP,
	OK+X_SG_CON,    X_SG_DAT, OK+X_SG_MXU, OK+X_SG_MXP,
	OK+X_SG_DAT,    X_SG_DAT,    X_SG_DAT,    X_SG_MXU,
	   X_SG_DAT, OK+X_SG_DAT, OK+X_SG_MXU,    X_SG_MXU,
	OK+X_SG_MXU, OK+X_SG_MXU, OK+X_SG_MXU, OK+X_SG_MXU,
	OK+X_SG_MXU, OK+X_SG_MXU, OK+X_SG_MXU, OK+X_SG_MXU,
	OK+X_SG_MXP,    X_SG_MXU,    X_SG_MXU, OK+X_SG_MXP,
	OK+X_SG_MXP,    X_SG_MXU,    X_SG_MXU, OK+X_SG_MXP
};

/* table marking the types with initialized data */

char	idata[] = { 0, 0, 0, 1, 1, 1, 1, 1 };

/*	control for preassigned segment types */

char	prelist[64];		/* preassigned segment type list */

int	prex;			/* number of entries in prelist */


/* miscellaneous variables */

char	*objname = {"x.out"},	/* name of object module */
	idbuf[32],		/* id's collected here */
	libname[64],		/* library names "/lib/libxx.a" built here */
	*fname,			/* current file name being processed */
	*ap,			/* appending pointer */
	line[128],		/* input read here */
	buf[512];		/* source to object copies done here */

short	objfile,		/* number of object file */
	objifile,		/* number of object input file (for reloc) */
	relfile,		/* number of relocation file */
	symfile,		/* number of symbol file */
	infile,			/* the current input file */
	errcnt,			/* count of errors */
	udefcnt,		/* current count of undefines */
	debug,			/* familiar debugging flag */
	*hpos,			/* where in the hash table sym is to go */
	seg,			/* loading segmented files */
	nonseg,			/* loading non-segmented files */
	splitid,		/* generate a slip-id module flag */
	shared,			/* generate a shared code flag */
	mapf,			/* produce a load map */
	rlo,			/* number of relocation items in buffer */
	dirty,			/* the relocation buffer is "dirty" */
	blen,			/* length of the relocation buffer */
	strip,			/* strip the output file flag */
	objnseg,		/* object number of segments */
	nolocal,		/* do not save local symbols */
	saverel;		/* keep the relocation info flag */

uns	segrel[SEGNO],		/* relocation values for this segment */
	segsiz[SEGNO];		/* current size of this segment */

long	stacksiz,		/* size of the stack segment */
	segpos[SEGNO],		/* file positions for this module */
	lseek(),		/* lseek now returns file pointer */
	relpos,			/* relocation data position in obj file */
	relval,
	relvalc,		/* code relocator for code */
	bpos = {1};		/* the position of the relocating buffer */


main(argc,argv) char **argv; {

reg short	i;

	if( argc < 2 ){
		fprintf(stderr,"usage:  ld8k [flags]  file [file...]\n");
		exit(0);
	}
	for( i=0; i < MAPNO; i++ ) map[ i ] = 255; /* set map  to unknown */

	pass0(argc,argv);
	pass1();
	interlude();
	if( mapf ) loadmap();
	if( udefcnt && !saverel ){
		err("\n%d undefined symbols:\n\n",udefcnt);
		for( i=1; i<symlev; i++ )
			if( symtab[i].x_sy_fl == X_SY_UNX )
				fprintf(stderr,"%-8.8s\n",symtab[i].x_sy_name);
	}
	if( errcnt ) errex( "pass1" );
	objfile = mktemp( objname );	/* This is NOT a temporary! */
	if( objfile < 0 ) errex( "can't create %s" , objname );
	objifile = objfile;		/* Use same file for relocation */
	if( saverel ) relfile = mktemp( "relxxxx" );
	pass2();
	if( errcnt ){
		close( objfile );
		creatb( objname , 0666 );
		errex( "pass2" );
	}
	finale();
	exit( 0 );
}


pass0(argc,argv) char **argv; {

reg short	i,
		j;
reg char	*cp;

	for( i=1; i<argc; i++ ){
		cp = argv[i];
		if( *cp == '-' )for(;;){
			switch( *++cp ){

case 0:				break;
case 'd':			/* enter segment descriptive information */
				if( *++cp )	dodline( cp );
					else	dodfile( argv[ ++i ] );
				break;
case 'h':			debug++;  continue;
case 'i':			splitid++;  continue;
case 'l':			if( filndx >= FILNO ) errex( "too many files" );
				*--cp = '-';
				filtab[filndx++].filname = cp;
				break;
case 'm':			mapf++;  continue;
case 'n':			shared++;  continue;
case 'o':			objname = argv[++i];  continue;
case 'r':			saverel++;  continue;
case 't':			setstack( argv[++i] );  continue;
case 's':			strip++;  continue;
case 'u':			j = lookup(symhash,SYMHASH,argv[++i]);
				if( j == -1 ){
					*hpos = install( argv[i] );
					udefcnt++;
				}
				continue;
case 'w':				/* preassign the first 4 segments */
				segtab[0].segtyp = X_SG_COD;
				segtab[1].segtyp = X_SG_CON;
				segtab[2].segtyp = X_SG_DAT;
				segtab[3].segtyp = X_SG_BSS;
				continue;
case 'x':			nolocal++;  continue;

default:			err( "bad flag: '%c'" , *cp );
				continue;
			}
			break;
		} else {
			/* regular file */
			if( filndx >= FILNO ) errex( "too many files" );
			filtab[ filndx ].filname = cp;
			filndx++;
		}
	}
}


pass1(){

reg short	i,
		j;
reg uns		k;

	for( i=0; i < filndx; i++ ){  /* process all files */
		fname = filtab[ i ].filname;
		if( fname[ 0 ] == '-' ){  /* -lxx */
			ap = libname;
			append( "lib" );
			append( fname+2 );
			append( ".a" );
			infile = openb( libname, 0 );
			if( infile < 0 ){
				err( "can't open %s" , libname );
				continue;
			}
		} else {
			infile = openb( fname , 0 );
			if( infile < 0 ){
				err( "can't read %s" , fname );
				continue;
			}
		}
		p1entry( i );
		close( infile );
	}
	if( !saverel && udefcnt ){  /* allocate the bss symbols */
		for( i=1; i<symlev; i++ ){
			if( symtab[i].x_sy_fl == X_SY_UNX &&
				(k = symtab[i].x_sy_val) != 0 ){
				j = findseg( X_SG_BSS , k );
				segtab[j].segtyp = newtyp(segtab[j].segtyp,X_SG_BSS);
				if( k > 1 ) round( segtab[j].segsiz );
				symtab[i].x_sy_val = segtab[j].segsiz;
				symtab[i].x_sy_fl = X_SY_GLB;
				symtab[i].x_sy_sg = j;
				segtab[j].segsiz += k;
				udefcnt--;
			}
		}
	}
	for( i=0; i<SEGNO; i++ ) round( segtab[i].segsiz );
	if( stacksiz ){		/* allocate a stack segment */
		j = findseg( X_SG_STK, stacksiz );
		segtab[j].segtyp = newtyp(segtab[j].segtyp,X_SG_STK);
		segtab[j].segsiz += stacksiz;
	}
}


p1entry( n ){  /* process file # n */

reg short	i;
long		fpos;

	if( !rdhead() ) return;  /* can't read header */
	if( x_hdr.x_magic == AR8KMAGIC ){  /* an archive! */
		if( udefcnt == 0 ) return;  /* nothing to resolve */
		i = sizeof( short );
		fpos = i;
		i = lseek( infile, fpos, 0 );
		while( ((i = read( infile, &ar8k_hd, AR8SIZ)) == AR8SIZ )
		    && ( ar8k_hd.ar8k_name[0] != '\0') ){
			if( trysim() ){  /* entry satisfied some need */
				if( filtab[ n ].posndx <= 0 )
					filtab[ n ].posndx = posmax+1;
				postab[ ++posmax ] = fpos + AR8SIZ;
				lseek( infile, fpos + AR8SIZ, 0 );
				if( !rdhead() ) return; /* oops */
				p1load();
				if( udefcnt == 0 ) return;  /* done */
			}
			fpos += AR8SIZ + ar8k_hd.ar8k_size;
			round( fpos );
			lseek( infile, fpos, 0 );
		}
		if( filtab[n].posndx > 0 ) ++posmax;
		return;
	}
	if( symread() == 0 ) return;
	filtab[ n ].posndx = -1;  /* not an archive */
	lseek( infile, (long)XHSIZ, 0 );
	p1load();
}


p1load(){ /* this does the pass1 processing for a file,  the symbol
		table has now been read and so has the header */
register i;


	if( (x_hdr.x_magic & 0xfff0) != X_SU_MAGIC ){
		err("bad magic in %s",fname);
		return;
	}

/* check the file type for compatibility */

	if( (x_hdr.x_magic & 0xf) < (X_NU_MAGIC & 0xf ) ){ /* seg magic */
		if( nonseg ){  /* already the other type */
			err("%s is a segmented file in a non-segmented load",
				fname);
			return;
		}
		seg++;
	} else {
		if( seg ){ /* alread the other type */
			err("%s is a non-segment file in a segmented load",
				fname);
			return;
		}
		nonseg++;
	}

	p1doseg();		/* now read the segment headers */
	p1dosym();		/* process the symbol table */
}


p1doseg(){  /* this processes all the segment headers and does allocation */

reg short	i,
		j,
		k;
reg struct x_sg *xp;		/* scanning pointer for seg headers */

	i = x_hdr.x_nseg * sizeof( struct x_sg );
	if( read( infile, x_sg, i ) != i ){
		err("bad segment header read in %s", fname );
		return;
	}
	if( modno >= MODNO ) errex("too many modules at %s",fname);
	modmap[ modno ] = maps = mapx;		/* start of the map index */
	modsym[ modno++ ] = syms = symx;	/* start of the symbol index */
	mapx += x_hdr.x_nseg;
	symx += (symtop-symlev);
	if( mapx > MAPNO ) errex("map overflow in %s",fname);
	if( symx > SYMPNO ) errex("symap overlow in %s",fname);

/* now process the symbol table to find preallocated segments */

	for( i = symlev; i < symtop; i++ ){
		if( symtab[ i ].x_sy_fl == X_SY_SEG ){ /* segment name */
			j = lookup( seghash, SEGHASH, symtab[i].x_sy_name );
			if( j != -1 ){/* seg already exists, give it same # */
				map[maps+symtab[i].x_sy_sg] = symtab[j].x_sy_sg;
				symap[syms + (i-symlev) ] = j;
			}
		}
	}

/* now process all the segments */

	for( xp = x_sg, k = 0; k < x_hdr.x_nseg; xp++, k++ ){
		j = xp->x_sg_no & 0177;
		if( j == 0177 ) j = findseg( xp->x_sg_typ , xp->x_sg_len);
		segtab[j].segtyp = newtyp( segtab[j].segtyp, xp->x_sg_typ);
		map[maps + k ] = j;
		segrel[ k ] = segtab[j].segsiz;
		if( ( segtab[j].segsiz += xp->x_sg_len) >= 65536L )
			err( "segment %d overflow in %s",j,fname);
		round( segtab[j].segsiz );  /* round size */
	}
}


p1dosym(){  /* process all the symbols in the symbol table for this module */

reg short	i,
		j,
		k;
short		symstt;
short		discard;
short		*syp;

/* now relocate the symbols in the symbol table */

	symstt = symlev;
	for( i=symlev; i < symtop; i++ ){
		syp = &symap[ syms + (i-symstt) ];
		*syp = symlev;
		j = symtab[i].x_sy_fl;
		switch( j ){

case X_SY_SEG:		k = lookup( seghash, SEGHASH, symtab[i].x_sy_name );
			if( k == -1 ) break;
			*syp = k;
			continue;

case X_SY_LOC:		hpos = &discard;
			if( nolocal ) continue;  /* don't save local syms */
			k = symtab[i].x_sy_sg & 0377;
			if( !saverel && k != 255 ){  /* final link only: relocate like a global, but not absolute */
				symtab[i].x_sy_val += segrel[ k ];
				symtab[i].x_sy_sg = map[ maps + k ]; /* fix seg no */
			}
			break;

case X_SY_UNX:		k = lookup( symhash, SYMHASH, symtab[i].x_sy_name );
			if( k != -1 ){  /* already referenced */
				if( symtab[k].x_sy_val < symtab[i].x_sy_val )
					symtab[k].x_sy_val = symtab[i].x_sy_val;
				*syp = k;
				continue;
			}
			udefcnt++;
			break;

case X_SY_GLB:		k = symtab[i].x_sy_sg;
			symtab[i].x_sy_val += segrel[ k ];
			symtab[i].x_sy_sg = map[ maps + k ]; /* fix seg no */
			k = lookup( symhash, SYMHASH, symtab[i].x_sy_name );
			if( k != -1 ){  /* already in table, check for compat */
				*syp = k;
				if( symtab[k].x_sy_fl == X_SY_UNX ){
					udefcnt--;  /* defined another one */
					symtab[ k ] = symtab[ i ];
				} else {
					err("multiple def: %-8s in %s",
						symtab[i].x_sy_name, fname );
				}
				continue;
			}
		}
		*hpos = symlev;
		symtab[ symlev++ ] = symtab[ i ];
	}
}


trysim(){  /* check a file to see if it should be loaded */

reg short	i,
		k;

	if( !rdhead() ) return( 0 );
	i = x_hdr.x_magic;
	if( seg && i != X_SU_MAGIC && i != X_SX_MAGIC ) return( 0 );
	if( nonseg && ( i==X_SU_MAGIC || i==X_SX_MAGIC) ) return( 0 );
	if( !symread() ) return( 0 );  /* no symbol table */
	for( i=symlev; i < symtop; i++ ){
		if( symtab[ i ].x_sy_fl == X_SY_GLB ){
			k = lookup( symhash, SYMHASH, symtab[i].x_sy_name );
			if( k != -1 && symtab[k].x_sy_fl == X_SY_UNX )
				return( 1 );  /* satisfied a ref */
		}
	}
	return( 0 );
}


symread(){  /* read the symbol table */

reg short	i;

/*	Note -- at this point the header has just been read  */

	symtop = symlev;
	if( lseek( infile, x_hdr.x_nseg * sizeof( struct x_sg) +
				x_hdr.x_init +
				x_hdr.x_reloc, 1) == -1 ){
		err("symbol seek failed in %s",fname);
		return( 0 );
	}
	symtop = symlev + x_hdr.x_symb/sizeof( struct x_sym );
	if( symtop > SYMNO ) errex("symbol table overflow in %s",fname);
	i = x_hdr.x_symb;
	if( read(infile, &symtab[symlev], i) != i ){
		err( "bad symbol table read in %s", fname );
		return( 0 );
	}
	return( 1 );
}


interlude(){  /* this does the segment allocation in the object file */

register	i,
		j;

	relvalc = relval = 0;			/* relocation bias in pass2 */
	if( !nonseg || saverel ) splitid = 0;	/* makes no sense */
	j = 0;
	for( i=0; i<SEGNO; i++ )
		if( segtab[i].segsiz ) j++;
	objnseg = j;				/* number of object segs */
	relpos = XHSIZ + j * sizeof( struct x_sg ); /* start of segments */
	for( i=0; i<prex; i++ )		/* first allocate preassigned */
		if( segtab[ prelist[i] ].segsiz ) interpos( prelist[i] );
	for( i=0; i<SEGNO; i++ )	/* then add the unassigned */
		if( segtab[i].segsiz ) interpos( i );
	if( relvalc >= 65536L ) err("code too big for non-seg load");
	if( relval >= 65536L ) err("too big for non-seg load");
	if( nonseg && !saverel )	/* do additional symbol table fix */
		for( i=1; i<symlev; i++ )
			if( (symtab[i].x_sy_fl & (X_SY_GLB & X_SY_LOC))
			    && (symtab[i].x_sy_sg & 0377) != 255 )
				symtab[i].x_sy_val +=
					segtab[ symtab[i].x_sy_sg & 0377 ].segrel;

	if( debug ) segdmp("end of interlude");
}


interpos( i ) register i; {

	if( idata[ segtab[i].segtyp] ){
		segtab[i].segpos = relpos;
		relpos += segtab[i].segsiz;
	}
	if( splitid && segtab[i].segtyp == X_SG_COD ){
		segtab[i].segrel = relvalc;
		relvalc += segtab[i].segsiz;
	} else {
		segtab[i].segrel = relval;
		if( nonseg && !saverel ) /* add up reloc vals */
			relval += segtab[i].segsiz;
	}
	segtab[i].hiwat = segtab[i].segsiz;
	segtab[i].segsiz = 0;
}


loadmap(){  /* produce a load map */

reg short	i,
		j;

	fprintf( stdout, "load map for %s\n\nsegment sizes:\n\n",objname );
	for( i=0; i<SEGNO; i++ )
		if( segtab[i].hiwat )
			fprintf( stdout,
				"seg[%d]  %d  %8ld  %8ld\n",
				i,
				segtab[i].segtyp,
				(long)segtab[i].hiwat,
				segtab[i].segpos);

	fprintf( stdout, "\nsymbols:\n\n" );
	for( i=1; i < symlev; i++ )
		fprintf( stdout, "%3d %-8s  %3d  %d  %4x\n",
			i,
			symtab[i].x_sy_name,
			symtab[i].x_sy_sg & 0377,
			symtab[i].x_sy_fl,
			symtab[i].x_sy_val );
}


pass2(){

reg short	i;

	modno = 0;
	for( i=0; i < filndx; i++ ){
		fname = filtab[ i ].filname;
		if( filtab[ i ].posndx ){  /* load this one */
			if( fname[0] == '-' ){  /* an abbreviated name */
				ap = libname;
				append( "lib" );
				append( fname+2 );
				append( ".a" );
				infile = openb( libname, 0 );
				if( infile < 0 ){  /* phase error */
					errex("p2 can't open %s",fname);
				}
			} else {
				infile = openb( fname , 0 );
				if( infile < 0 )
					errex( "p2 can't open %s", fname );
			}
			p2read( i );
			close( infile );
		}
	}
}


p2read( n ){  /* read the files the second time */

reg short	i;

	i = filtab[ n ].posndx;
	if( i == 0 ) return;		/* nothing to do */
	if( i > 0 ){			/* an archive */
		while( postab[ i ] ){
			lseek( infile, postab[ i++ ], 0 );
			p2load();
		}
		return;
	}
	p2load();
}


p2load(){  /* load one file */

reg short	i,
		j;
reg struct x_sg *xp;
reg uns		l;
short		k;

	if( !rdhead() )return;  /* can't read header */
	i = x_hdr.x_nseg * sizeof( struct x_sg );
	if( read( infile, x_sg, i ) != i )
		errex("bad seg read in p2 %s", fname );
	maps = modmap[ modno ];
	syms = modsym[ modno++ ];
	for( xp=x_sg, i=0; i < x_hdr.x_nseg; xp++, i++ ){
		j = map[ maps + i ];  /* final seg */
		segsiz[ i ] = segtab[ j ].segsiz;
		segrel[ i ] = segtab[ j ].segrel + segtab[ j ].segsiz;
		segpos[ i ] = segtab[ j ].segpos;
		segtab[ j ].segsiz += xp->x_sg_len;
		round( segtab[ j ].segsiz );
		if( idata[ xp->x_sg_typ ] ){

			lseek( objfile, segpos[ i ], 0 );
			l = xp->x_sg_len;
			round( l );
			segtab[ j ].segpos += l;
			bpos = 1;	/* Invalidate reloc buffer */
			while( l ){
				k = l > 512 ? 512 : l;
				if( read( infile, buf , k ) != k)
					errex("bad read in %s",fname);
				if( write( objfile, buf, k ) != k)
					errex("bad write in %s",objname);
				l -= k;
			}
		}
	}
	p2dorel();			/* do the relocation */
}


p2dorel(){  /* do the entire relocation section of file */

long	l;
reg short	i;
reg struct x_rel *rlp;

	l = x_hdr.x_reloc;
	while( l ){
		i = l > RSIZ ? RSIZ : l;
		if( read( infile, rlibuf, i ) != i )
			errex("bad relocation read in %s",fname);
		l -= i;
		rlp = rlibuf;
		while( i > 0 ){
			reloc( rlp );
			i -= sizeof( struct x_rel );
			rlp++;
		}
	}
	flshbuf();
}


reloc( rlp ) reg struct x_rel *rlp; {  /* relocate a single item */

reg short	seg,
		j,
		*ip;
reg uns		val;
long		pos;

	if( rlp->x_rl_flg & 04 ){  /* external item in symbol table */
		j = symap[ syms + rlp->x_rl_bas ];
		seg = 127;
		if( saverel ){
			val = 0;
			rlp->x_rl_bas = j-1;
		} else {
			val = symtab[ j ].x_sy_val;
			j = symtab[ j ].x_sy_sg & 0177;
			if( j != 127 ) seg = map[ maps + j ];
		}
	} else {
		j = rlp->x_rl_bas;
		seg = map[ maps + j ];
		val = segrel[ j ];
		if( saverel ) rlp->x_rl_bas = seg;
	}
	pos = segpos[ rlp->x_rl_sgn ] + rlp->x_rl_loc;
	if( saverel ){
		rlp->x_rl_loc += segsiz[ rlp->x_rl_sgn ]; 
		rlp->x_rl_sgn = map[maps+rlp->x_rl_sgn];
		putrel( rlp );
	}
	j = pos & 511;
	ip = (short *)&buf[ j ];
	getbuf( pos );
	dirty++;
	switch( rlp->x_rl_flg & 03 ){  /* what is the relocation target */

case X_RL_OFF:	*ip += val;		/* relocate a 16 bit simple offset */
		break;

case X_RL_SSG:	val += *ip & 0377;	/* relocate a short seg plus offset */
		if( val > 255 ) err("short segment offset overflow in %s",fname);
		*ip = ( seg << 8 ) | val;
		break;

case X_RL_LSG:				/* relocate a long seg plus offset */
		*ip = 0100000 | (seg << 8);	/* set the segment number */
		pos += 2;
		getbuf( pos );
		ip = (short *)&buf[ pos & 511 ];
		*ip += val;
		dirty++;
	}
}


finale(){  /* do the final clean up,  the essential portion of the
		file has been written, but not yet the header, which
		will be written last -- next goes the relocation stuff
		which has been put into a side file -- then the symbol
		table, and then at last the header */

reg short	i,
		j;
reg char	*cp;
reg uns		*ip;
reg struct x_rel *rlp;

	x_hdr.x_magic = (udefcnt == 0);  /* executability bit */
	x_hdr.x_magic |= nonseg ?
				(splitid ? X_NUI_MAGIC : X_NU_MAGIC) :
				X_SU_MAGIC;
	x_hdr.x_nseg = objnseg;
	x_hdr.x_init = relpos - XHSIZ - objnseg * sizeof( struct x_sg );
	x_hdr.x_reloc = 0;
	x_hdr.x_symb = strip && !saverel ? 0 : (symlev-1)*sizeof(struct x_sym);
	
	if(debug)segdmp( "finale" );

		/* here put out the segment descriptors */

	lseek( objfile, (long)XHSIZ, 0 );
	for( j=0; j<prex; j++ ) finout( prelist[j] );
	for( i=0; i<SEGNO; i++ ) finout( i );

		/* now put out the relocation file */

	lseek( objfile, relpos, 0 );
	if( saverel ){	/* copy the relocation file and fix segment numbers */
		wrrel();
		lseek( relfile, 0L, 0 );
		while( (i = read( relfile, rlibuf, RSIZ )) > 0 ){
			x_hdr.x_reloc += i;
			write( objfile, rlibuf, i );
		}
	}

		/* then the symbol table */

	write( objfile, &symtab[1], (short)x_hdr.x_symb );

		/* and at last the header */

	lseek( objfile, 0L, 0 );
	write( objfile, &x_hdr, XHSIZ );
	close( objfile );
}


finout( i ) register i; {	/* write an x.out segment descriptor */

	if( segtab[i].hiwat ){
		x_sg[0].x_sg_no = saverel ? 255 : i;
		x_sg[0].x_sg_typ = segtab[i].segtyp;
		x_sg[0].x_sg_len = segtab[i].hiwat;
		write( objfile, x_sg, sizeof( struct x_sg ));
		segtab[i].hiwat = 0;
	}
}


dodline( s ) reg char *s; {

reg char	*cp;
reg short	i,
		j,
		k;
int		ch;
struct x_sym	*syp;
char		*ssave;

	ssave = s;			/* save for possible error print */
	j = i = 0;
	if( isalpha( *s ) ){		/* process format: type=n,n,n */
		cp = idbuf;
		while( isalpha( *s ) ){
			i = *s++;
			if( cp < &idbuf[XNAMELN] ) *cp++ = i;
		}
		while( cp < &idbuf[XNAMELN] ) *cp++ = 0;
if(debug)fprintf(stderr,"idbuf = %s\n",idbuf);
		if( idmatch( idbuf, "text" ) ) j = X_SG_COD; else
		if( idmatch( idbuf, "data" ) ) j = X_SG_DAT; else
		if( idmatch( idbuf, "bss" ) ) j = X_SG_BSS; else
		if( idmatch( idbuf, "cons" ) ) j = X_SG_CON; else
			goto illdesc;
		if( *s != '=' ) goto illdesc;
		ch = ',';
		while( ch == ',' ){
			s++;
			if( !isdigit( *s ) ) goto illdesc;
			i = 0;
			while( isdigit( *s ) ) i = i*10 + *s++ - '0';
			ch = *s;
			k = i;
			if( ch == '-' ){	/* range */
				s++;
				if( !isdigit( *s ) ) goto illdesc;
				k = 0;
				while( isdigit( *s ) ) k = k*10 + *s++ - '0';
				ch = *s;
			}
			if( i < 0 || i > SEGNO || k < 0 || k > SEGNO )
					goto illdesc;
			for(;;){
				dodset( i, j );
				if( i == k ) break;
				if( i < k ) i++; else i--;
			}
		}
		if( ch != 0 ) goto illdesc;
		return;
	}
	if( !isdigit( *s ) ) goto illdesc;
	while( isdigit( *s ) ) i = i*10 + *s++ - '0';
	if( i < 0 || i >= SEGNO )goto illdesc;
	j = 0;
	while( *s == ',' ){		/* preallocate a segment type */
		s++;
		switch( *s ){
	case 0:		goto illdesc;
	case 'T':
	case 't':	j = X_SG_COD;  break;
	case 'D':
	case 'd':	j = X_SG_DAT;  break;
	case 'C':
	case 'c':	j = X_SG_CON;  break;
	case 'B':
	case 'b':	j = X_SG_BSS;  break;
	default:	goto illdesc;
		}
		s++;
		dodset( i , j );
		j = 0;
	}

		/* more of dodline */


next:
	dodset( i , j );		/* allocate segment */
	if( *s == 0 ) return;
	if( *s != '=' )goto illdesc;
	ch = ',';
	while( ch == ',' ){
		s++;
		cp = idbuf;
		while( *s && *s != ',' ) *cp++ =  *s++;
		while( cp < &idbuf[XNAMELN] ) *cp++ = 0;
		ch = *s;
		j = lookup( seghash , SEGHASH, idbuf);
		if( j == -1 ) *hpos = j = install( idbuf );
		syp = &symtab[ j ];
		j = syp->x_sy_sg & 0177;
		if( j != 127 && j != i ){
			err( "multiple definition of segment: %s" , idbuf );
			goto illdesc;
		}
		syp->x_sy_sg = i;
		syp->x_sy_fl = X_SY_SEG;
		j = 0;
		if( idmatch( idbuf, "_text" ) ) j = X_SG_COD; else
		if( idmatch( idbuf, "_data" ) ) j = X_SG_DAT; else
		if( idmatch( idbuf, "_bss" ) ) j = X_SG_BSS; else
		if( idmatch( idbuf, "_cons" ) ) j = X_SG_CON;
		if( j ) segtab[i].segtyp = newtyp( segtab[i].segtyp, j );
	}
	if( ch == 0 ) return;

illdesc:
	err( "descriptor format" );
	fprintf(stderr,"\t%s\n\t",ssave);
	j = cp - line;
	while( --j >= 0 ) putc( ' ' , stderr );
	putc( '|', stderr );
	putc( '\n', stderr );
}


dodset( i, typ ) register i, typ; {

	if( !segtab[i].segalloc ){
		prelist[ prex++ ] = i;
		segtab[i].segalloc = 1;
	}
	if( typ ) segtab[i].segtyp = newtyp(segtab[i].segtyp, typ );
}



dodfile( s ) char *s; {  /* enter an entire descriptor file */

reg char	*cp;
reg int	i;

	close( 0 );
	if( opena( s , 0666 ) != 0 ){
		err( "can't open descriptor file: %s", s );
		return;
	}
	cp = line;
	while( (i = getchar()) != EOF ){
		if( i != ' ' ){		/* squeeze out blanks */
			if( i == '\n' ){
				*cp = 0;
				dodline( cp = line );
			} else	*cp++ = i;
		}
	}
	if( debug ){
		fprintf(stderr,"preallocation list:\n" );
		for( i=0; i<prex; i++ )
			fprintf(stderr,"prelist[%d] = %d\n",i,prelist[i]);
	}
}


append( s ) reg char *s; { /* append to the pointer 'ap' */

	while( *ap++ = *s++ );
	ap--;
}


err( s , a , b , c ) char *s; {

	fprintf( stderr , "error: " );
	fprintf( stderr , s , a , b , c );
	fprintf( stderr , "\n" );
	errcnt++;
}

errex( s , a , b , c ) char *s; {

	fprintf( stderr , "fatal " );
	err( s , a , b , c );
	exit( 1 );
}


findseg( typ, siz ) uns siz; {  /* find a segment to fit this in */

reg short	i,
		j;

	for( j=0; j<prex; j++ ){	/* look for preallocated first */
		i = prelist[ j ];
		if(	oktyp( segtab[i].segtyp, typ ) &&
			segtab[i].segsiz + siz < 65536L )
				return( i );
	}
	for( i=0; i<SEGNO; i++ )
		if(	oktyp( segtab[i].segtyp, typ) &&
			!segtab[i].segalloc &&
			segtab[i].segsiz + siz < 65536L )
				return( i );
	errex("segment overflow in %s",fname);
}


flshbuf(){  /* flush the buffer used for relocation reads and writes */

	if( dirty ){
		lseek( objfile, bpos, 0 );
		write( objfile, buf, blen );
		dirty = 0;
		bpos = 1;
	}
}


getbuf( p ) long p; {  /* fetch a relocation buffer */

reg i;

	p &= -512;
	if( bpos != p ){
		if( dirty ) flshbuf();
		bpos = p;
		lseek( objifile, bpos, 0 );
		blen = read( objifile, buf, 512 );
	}
}


idmatch( a , b ) reg char *a, *b; {

reg int	i,
		j;

	for( i=0; i<XNAMELN; i++ ){
		j = *a++;
		if( j == 0 ) return( *b == 0 );
		if( j != *b++ ) return( 0 );
	}
	return( 1 );
}


install( s ) reg char *s; {

reg char	*cp;
reg int	i;

	i = symlev;
	if( i >= SYMNO ) errex( "symbol table overflow" );
	symtab[ i ].x_sy_sg = 255;
	symtab[ i ].x_sy_fl = X_SY_UNX;
	cp = symtab[ i ].x_sy_name;
	while( *cp++ = *s++ );
	symlev++;
	return( i );
}


lookup( htab , HTAB , sym ) int htab[]; char *sym; {

reg int	i,
		j;
reg char	*cp;
int		k;

	i = 0;
	cp = sym;
	for(k = 0; k < XNAMELN && *cp; k++) i = i*5 + *cp++;
	i &= HTAB-1;
	for( k=0; k<2; k++ ){
		while( i < HTAB ){
			if( ( j = htab[ i ]) ){
				if( idmatch( sym, symtab[ j ].x_sy_name ))
					return( j );
			} else {  /* not there */
				hpos = &htab[ i ];
				return( -1 );
			}
			i++;
		}
		i = 0;
	}
	errex( "hash table overflow: %d", HTAB );
}


mktemp( s ) char *s; {  /* simple version */

reg short	i;

	close( creatb( s , 0666 ) );
	i = openb( s , 2 );		/* for read and write */
	return( i );
}


putrel( rlp ) reg struct x_rel *rlp; {  /* output a relocation item */

	if( rlo >= RLC ) wrrel();
	rlobuf[ rlo++ ] = *rlp;
}


rdhead(){  /* read an x.out header into the header buffer */

	if( read( infile, &x_hdr, XHSIZ ) == XHSIZ ){
	if(debug>2){
		fprintf(stderr,
			"%s: magic = %x nseg = %d init = %ld reloc = %ld symb = %ld\n",
			fname, x_hdr.x_magic, x_hdr.x_nseg,
			x_hdr.x_init, x_hdr.x_reloc, x_hdr.x_symb);
		}
		return( 1 );
	}
	err( "unexpected EOF while reading header on %s", fname );
	return( 0 );
}


setstack( s ) reg char *s;{

long	l;

	while( *s >= '0' && *s <= '9' )
		l = l * 10 + ( *s++ - '0' );
	if( l & 1 ) l++;
	if( l < 0 || l > 65536 ){
		err( "stack segment too big" );
		return;
	}
	round( l );
	stacksiz = l;
}


wrrel(){  /* flush the relocation output buffer */

	if( rlo ){
		write( relfile, rlobuf, rlo * sizeof( struct x_rel ) );
		rlo = 0;
	}
}



segdmp(s) char *s;{  /* dump all segment data */

reg short i;

	fprintf( stderr,"segdmp called from %s\n" ,s);
	fprintf( stderr,"old:new typ siz hiwat segrel pos\n" ,s);
	for( i=0; i<SEGNO; i++ )
		if( segtab[i].segsiz || segtab[i].hiwat || segtab[i].segpos )
			segsho( 0 , i );
}

segsho( s , i ) char *s; {

	if( s ) fprintf( stderr , s );
	fprintf( stderr,
		"seg[%d]:%d %d  %ld  %ld  %ld  %ld\n",
		i,
		segtab[i].segalloc,
		segtab[i].segtyp,
		segtab[i].segsiz,
		(long)segtab[i].hiwat,
		(long)segtab[i].segrel,
		segtab[i].segpos);
}
