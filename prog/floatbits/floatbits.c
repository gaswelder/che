// prints bits of float64 numbers.
int main() {
	printfloat(-OS.INFINITY);
	printfloat(OS.INFINITY);
	printfloat(0);
	printfloat(1);
	printfloat(2);
	printfloat(1024);
	printfloat(0.25);
	printfloat(0.25 + 0.00000000000000005);
	printfloat(100000);
	return 0;
}

typedef {
	uint8_t *bytep;
	size_t bytepos;
} bitreader_t;

int nextbit(bitreader_t *r) {
	int bit = (*r->bytep & (1<<r->bytepos)) >> r->bytepos;
	r->bytepos++;
	if (r->bytepos == 8) {
		r->bytep++;
		r->bytepos = 0;
	}
	return bit;
}

void printfloat(double x) {
	void *p = &x;
	bitreader_t r = {p, 0};
	char bits[64] = {};
	for (int i = 0; i < 64; i++) {
		bits[i] = nextbit(&r);
	}

	printf("%30.20f\t", x);
	printf("%d|", bits[63]);

	int exp = 0;
	for (int i = 62; i >= 52; i--) {
		exp *= 2;
		exp += bits[i];
		printf("%d", bits[i]);
	}
	exp -= 1023;
	printf(" = %5d|", exp);

	uint64_t sig = 0;
	for (int i = 51; i >= 0; i--) {
		sig *= 2;
		sig += bits[i];
		printf("%d", bits[i]);
	}
	printf("=%zu\n", sig);
}
