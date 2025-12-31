#import bits
#import reader
#import test

int main() {
	testreader();
	testwriter();
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
		r = r * 2 + bits.bits_getn(b, 1);
	}
	test.truth("r == 128", r == 128);
	test.truth("EOF", bits.bits_getn(b, 1) == EOF);

	bits.closereader(b);
	reader.free(fr);
	fclose(f);
}

void testwriter() {
	FILE *f = tmpfile();
	bits.writer_t *w = bits.newwriter(f);

	bits.writebit(w, 0);
	bits.writebit(w, 0);
	bits.writebit(w, 1);
	bits.writebit(w, 1);
	bits.writebit(w, 1);
	bits.writebit(w, 0);
	test.truth("!err", w->err == false);

	test.truth("close", bits.closewriter(w) == true);
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
