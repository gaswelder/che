#import bitreader
#import table

// prints bits of float64 numbers.
int main() {
	table.table_t *tbl = table.new(4);
	table.pushval(tbl, "value");
	table.pushval(tbl, "sign");
	table.pushval(tbl, "exponent");
	table.pushval(tbl, "mantissa");

	printfloat(tbl, -OS.INFINITY);
	printfloat(tbl, OS.INFINITY);
	printfloat(tbl, 0);
	printfloat(tbl, 1);
	printfloat(tbl, 2);
	printfloat(tbl, 1024);
	printfloat(tbl, 0.25);
	printfloat(tbl, 0.25 + 0.00000000000000005);
	printfloat(tbl, 100000);

	table.print(tbl);
	table.free(tbl);
	return 0;
}

void printbits(char *buf, uint8_t *bits, size_t n) {
	for (size_t i = 0; i < n; i++) {
		if (bits[i] == 1) {
			buf[i] = '1';
		} else {
			buf[i] = '0';
		}
	}
}

void printfloat(table.table_t *tbl, double x) {
	parsed_double_t parsed = parse_double(x);
	char buf[64] = {};
	table.pushval(tbl, "%30.20f", x);
	table.pushval(tbl, "%d", parsed.sign);

	printbits(buf, parsed.exp_bits, 11);
	table.pushval(tbl, "[%s] (%d)", buf, parsed.exp);

	printbits(buf, parsed.sig_bits, 52);
	table.pushval(tbl, "[%s] (%zu)", buf, parsed.sig);
}

typedef {
	uint8_t bits[64];
	uint8_t sign;
	uint8_t exp_bits[11];
	uint8_t sig_bits[52];
	int exp;
	uint64_t sig;
} parsed_double_t;

parsed_double_t parse_double(double x) {
	parsed_double_t parsed = {};

	// Look at x as a sequence of bytes.
	void *p = &x;
	uint8_t *bytes = p;

	// Unpack the bits.
	uint8_t bits8[8] = {};
	for (int i = 0; i < 8; i++) {
		bitreader.getbits_lsfirst(bytes[i], bits8);
		for (int j = 0; j < 8; j++) {
			parsed.bits[8*i + j] = bits8[j];
		}
	}

	// The leftmost bit is the sign.
	parsed.sign = parsed.bits[63];

	// Next 11 bits is the exponent.
	for (int i = 62; i >= 52; i--) {
		parsed.exp *= 2;
		parsed.exp += parsed.bits[i];
		parsed.exp_bits[(i-52)] = parsed.bits[i];
	}
	parsed.exp -= 1023;

	// The rest is the normalized value.
	for (int i = 51; i >= 0; i--) {
		parsed.sig *= 2;
		parsed.sig += parsed.bits[i];
	}
	return parsed;
}
