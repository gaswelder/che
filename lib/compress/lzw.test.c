#import lzw.c
#import reader
#import rnd
#import test
#import writer

int main() {
	// seq 9
	uint8_t seq[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
	testrw(seq, sizeof(seq), 10);

	// random
	size_t n = 20000;
	uint8_t *text = calloc!(n, 1);
	rnd.seed(1);
	for (size_t i = 0; i < n; i++) {
		text[i] = 32 + rnd.intn(60);
	}
	testrw(text, n, 256);
	free(text);

	return test.fails();
}

void testrw(uint8_t *msg, size_t len, alphabet_size) {
	FILE *f = tmpfile();

	// Write a message.
	writemsg(f, msg, len, alphabet_size);

	// Read the message back.
	rewind(f);
	uint8_t *back = calloc!(len, 1);
	readmsg(f, back, len, alphabet_size);

	// Compare.
	test.truth("data == back", memcmp(msg, back, len) == 0);

	free(back);
	fclose(f);
}

void writemsg(FILE *f, uint8_t *data, size_t datalen, size_t alphabet_size) {
	reader.t *in = reader.static_buffer(data, datalen);
	writer.t *out = writer.file(f);
	lzw.compress(in, alphabet_size, out);
	reader.free(in);
	writer.free(out);
}

void readmsg(FILE *f, uint8_t *buf, size_t bufsize, size_t alphabet_size) {
	reader.t *in = reader.file(f);
	writer.t *out = writer.static_buffer(buf, bufsize);

	lzw.dec_t *dec = lzw.newdecoder(in, alphabet_size);
	lzw.decodeinto(dec, out);
	lzw.freedecoder(dec);

	reader.free(in);
	writer.free(out);
}
