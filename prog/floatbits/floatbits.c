#import bitreader

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

void printfloat(double x) {
	// Look at x as a sequence of bytes.
	void *p = &x;
	uint8_t *bytes = p;

	// Unpack the bits.
	char bits[64] = {};
	uint8_t bits8[8] = {};
	for (int i = 0; i < 8; i++) {
		bitreader.getbits_lsfirst(bytes[i], bits8);
		for (int j = 0; j < 8; j++) {
			bits[8*i + j] = bits8[j];
		}
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
