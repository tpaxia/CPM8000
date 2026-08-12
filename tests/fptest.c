union fbits {
	float f;
	long l;
};

int failures;

check(name, got, expected)
char *name;
long got, expected;
{
	if (got != expected) {
		printf("FAIL %s got=%08lx expected=%08lx\n", name, got, expected);
		failures++;
	}
}

main()
{
	float a, b;
	union fbits result;

	a = 1.5;
	b = 2.25;

	result.f = a + b;
	check("add", result.l, 0x40700000L);
	result.f = b - a;
	check("sub", result.l, 0x3f400000L);
	result.f = a * b;
	check("mul", result.l, 0x40580000L);
	result.f = b / a;
	check("div", result.l, 0x3fc00000L);

	if (!(a < b)) {
		printf("FAIL compare less\n");
		failures++;
	}
	if (a == b) {
		printf("FAIL compare equal\n");
		failures++;
	}

	result.f = (float) 7;
	check("int-to-float", result.l, 0x40e00000L);
	if ((int) result.f != 7) {
		printf("FAIL float-to-int got=%d expected=7\n", (int) result.f);
		failures++;
	}

	if (failures)
		printf("FPTEST FAIL %d\n", failures);
	else
		printf("FPTEST PASS\n");
}
