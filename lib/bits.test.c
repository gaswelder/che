#import bits
#import reader
#import test
#import writer

int main() {
	testreader();
	testwriter();
	testunpack();
	return test.fails();
}

void testreader() {
	FILE *f = tmpfile();
	fputc(128, f);
	fseek(f, 0, SEEK_SET);

	reader.t *fr = reader.file(f);
	bits.reader_t *b = bits.newreader(fr);
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

void testwriter() {
	FILE *f = tmpfile();

	// Write a sequence into a file.
	writer.t *fw = writer.file(f);
	bits.writer_t *w = bits.newwriter(fw);
	bits.write1(w, 0);
	bits.write1(w, 0);
	bits.write1(w, 1);
	bits.write1(w, 1);
	bits.write1(w, 1);
	bits.write1(w, 0);
	test.truth("!err", w->err == false);
	test.truth("close", bits.closewriter(w) == true);
	writer.free(fw);

	// Read the sequence back.
	fseek(f, 0, SEEK_SET);
	reader.t *fr = reader.file(f);
	bits.reader_t *b = bits.newreader(fr);
	test.truth("001110 00", bits.read1(b) == 0);
	test.truth("001110 00", bits.read1(b) == 0);
	test.truth("001110 00", bits.read1(b) == 1);
	test.truth("001110 00", bits.read1(b) == 1);
	test.truth("001110 00", bits.read1(b) == 1);
	test.truth("001110 00", bits.read1(b) == 0);
	test.truth("001110 00", bits.read1(b) == 0);
	test.truth("001110 00", bits.read1(b) == 0);
	test.truth("eof", bits.read1(b) == EOF);
	bits.closereader(b);
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