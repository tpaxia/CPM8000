/* x.out declarations used by putboot.c, without xout.h's global x_sg[]. */

struct x_hdr {
	short x_magic;
	short x_nseg;
	long x_init;
	long x_reloc;
	long x_symb;
};

struct x_sg {
	char x_sg_no;
	char x_sg_typ;
	unsigned x_sg_len;
};

#define X_SG_BSS 1
#define X_SG_STK 2
