/*
 * FPVEC -- run TestFloat-generated vectors through the Z8000 EPA emulator.
 *
 * Reads VEC.BIN, built on the host by tests/fpe/fpevec.py: a 32-byte header
 * (longs: magic, op, rounding mode, count) followed by one 32-byte record per
 * case: a, b, expected z (hi,lo each), expected flags, pad.  Mismatches are
 * printed as "BAD" lines for the host to triage.
 */
long ga[2], gb[2], gz[2];
int gmode, gfl, gcc;
long buf[128];

int (*ops[24])();
int zdbl[24];
extern int s_add(), s_sub(), s_mul(), s_div(), s_sqrt();
extern int d_add(), d_sub(), d_mul(), d_div(), d_sqrt();
extern int s_cmp(), d_cmp(), i_s(), i_d(), s_i(), d_i(), s_d(), d_s();
extern int s_rti(), d_rti();

main()
{
	int fd, n, i, i2, op, dbl, bad;
	long count, done, *r;

	ops[0] = s_add; ops[1] = s_sub; ops[2] = s_mul; ops[3] = s_div;
	ops[4] = s_sqrt;
	ops[5] = d_add; ops[6] = d_sub; ops[7] = d_mul; ops[8] = d_div;
	ops[9] = d_sqrt;
	/* s_eq s_le s_lt d_eq d_le d_lt */
	for (i = 10; i < 13; i++) ops[i] = s_cmp;
	for (i = 13; i < 16; i++) ops[i] = d_cmp;
	ops[16] = i_s; ops[17] = i_d; ops[18] = s_i; ops[19] = d_i;
	ops[20] = s_d; ops[21] = d_s; ops[22] = s_rti; ops[23] = d_rti;
	for (i = 5; i < 10; i++) zdbl[i] = 1;
	zdbl[17] = zdbl[20] = zdbl[23] = 1;

	fd = openb("VEC.BIN", 0);
	if (fd < 0) {
		printf("FPVEC: cannot open VEC.BIN\n");
		return;
	}
	if (read(fd, buf, 512) < 32 || buf[0] != 0x46505631L) {
		printf("FPVEC: bad header\n");
		return;
	}
	op = (int) buf[1];
	gmode = (int) buf[2];
	count = buf[3];
	dbl = zdbl[op];
	setmode();
	done = 0;
	bad = 0;
	/* the 32-byte header is followed by whole 32-byte records */
	r = buf + 8;
	n = (512 - 32) / 32;
	for (;;) {
		for (i = 0; i < n && done < count; i++, r += 8) {
			ga[0] = r[0]; ga[1] = r[1];
			gb[0] = r[2]; gb[1] = r[3];
			gz[0] = 0; gz[1] = 0;
			(*ops[op])();
			if (op >= 10 && op < 16) {
				/* comparison flags: eq 0x40, lt 0xa8, gt 0x08, unordered 0x10 */
				gcc &= 0xff;
				i2 = (op - 10) % 3;
				gz[0] = (gcc == 0x40 && i2 < 2) ||
				    (gcc == 0xa8 && i2 > 0);
			}
			done++;
			if (gz[0] != r[4] || (dbl && gz[1] != r[5]) ||
			    (gfl & 0xff) != (int) r[6]) {
				bad++;
				printf("BAD %lx %lx %lx %lx got %lx %lx %x exp %lx %lx %x\n",
				    ga[0], ga[1], gb[0], gb[1],
				    gz[0], gz[1], gfl & 0xff,
				    r[4], r[5], (int) r[6]);
			}
		}
		if (done >= count)
			break;
		i = read(fd, buf, 512);
		if (i < 32)
			break;
		r = buf;
		n = i / 32;
	}
	printf("FPVEC DONE %ld %d\n", done, bad);
}
