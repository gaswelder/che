#import bits
#import reader
#import test
#import writer

int main() {
	testunpack();
	testreader();

	uint8_t val[] = {0, 0, 1, 1, 1, 0, 0, 0};
	testwr(bits.STRAIGHT, val, nelem(val));
	testwr(bits.REVERSED, val, nelem(val));

	return test.fails();
}

void testreader() {
	FILE *f = tmpfile();
	fputc(128, f);
	fseek(f, 0, SEEK_SET);

	reader.t *fr = reader.file(f);
	bits.reader_t *b = bits.newreader(fr, bits.STRAIGHT);
	int r = 0;
	for (int i = 0; i < 8; i++) {
		r = r * 2 + bits.read1(b);
	}
	test.truth("r == 128", r == 128);
	test.truth("EOF", bits.read1(b) == EOF);

	bits.closereader(b);
	reader.free(fr);
	fclose(f);
}

void testwr(int order, uint8_t *val, size_t n) {
	FILE *f = tmpfile();

	// Write a sequence into a file.
	writer.t *fw = writer.file(f);
	bits.writer_t *w = bits.newwriter(fw, order);
	for (size_t i = 0; i < n; i++) {
		bits.write1(w, val[i]);
	}
	test.truth("!err", w->err == false);
	test.truth("close", bits.closewriter(w) == true);
	writer.free(fw);

	// Read the sequence back.
	fseek(f, 0, SEEK_SET);
	reader.t *fr = reader.file(f);
	bits.reader_t *br = bits.newreader(fr, order);
	for (size_t i = 0; i < n; i++) {
		int b = bits.read1(br);
		if (b < 0) panic("read failed");
		test.truth("001110 00", (uint8_t) b == val[i]);
	}
	test.truth("eof", bits.read1(br) == EOF);
	bits.closereader(br);
	reader.free(fr);

	fclose(f);
}

void testunpack() {
	uint8_t bits[8] = {};

	bits.getbits_msfirst(14, bits);
	test.truth("001110", bits[0] == 0);
	test.truth("001110", bits[1] == 0);
	test.truth("001110", bits[2] == 0);
	test.truth("001110", bits[3] == 0);
	test.truth("001110", bits[4] == 1);
	test.truth("001110", bits[5] == 1);
	test.truth("001110", bits[6] == 1);
	test.truth("001110", bits[7] == 0);

	bits.getbits_lsfirst(14, bits);
	test.truth("001110", bits[0] == 0);
	test.truth("001110", bits[1] == 1);
	test.truth("001110", bits[2] == 1);
	test.truth("001110", bits[3] == 1);
	test.truth("001110", bits[4] == 0);
	test.truth("001110", bits[5] == 0);
	test.truth("001110", bits[6] == 0);
	test.truth("001110", bits[7] == 0);
}