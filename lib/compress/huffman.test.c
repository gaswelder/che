#import bits
#import compress/huffman
#import reader
#import test
#import writer

pub int main() {
	testreadwrite();
	testcanonical();
	return test.fails();
}

void testreadwrite() {
	const char *s = "The quick brown fox jumps over the lazy dog.";
	huffman.tree_t *t = huffman.buildtree(s);

	// Write the message
	FILE *f = tmpfile();
	writemsg(f, t, s);

	// Read the message back
	rewind(f);
	size_t len = strlen(s);
	char *q = calloc!(len+1, 1);
	readmsg(f, t, q, len);
	test.streq(s, q);
	free(q);
	fclose(f);

	huffman.freetree(t);
}

void testcanonical() {
	// Canonical tree spec
	const char *characters = "TAHSND";
	uint8_t lencounts[] = {0, 3, 1, 2};

	// Encoded and plaintext message
	uint8_t bits[] = {0,1, 1,1,1,0, 1,1,1,1, 0,0, 1,0, 0,1, 0,0, 1,1,0, 0,0, 1,0, 0,1, 0,0};
	const char *msg = "ANDTHATSTHAT";

	// Write the encoded message
	FILE *f = tmpfile();
	writer.t *fw = writer.file(f);
	bits.writer_t *w = bits.newwriter(fw);
	bits.write(w, bits, nelem(bits));
	bits.closewriter(w);
	writer.free(fw);

	// Recreate the tree from the spec
	huffman.tree_t *t = huffman.treefrom(lencounts, nelem(lencounts), (uint8_t *) characters);

	// Read the message back
	rewind(f);
	char out[13] = {};
	readmsg(f, t, out, 12);
	fclose(f);

	test.streq(msg, out);
}

void writemsg(FILE *f, huffman.tree_t *t, const char *s) {
	writer.t *fw = writer.file(f);
	huffman.writer_t *w = huffman.newwriter(t, fw);
	const char *p;
	for (p = s; *p != '\0'; p++) {
		uint8_t x = *p;
		huffman.write(w, x);
	}
	huffman.closewriter(w);
	writer.free(fw);
}

void readmsg(FILE *f, huffman.tree_t *t, char *q, size_t len) {
	reader.t *fr = reader.file(f);
	bits.reader_t *br = bits.newreader(fr);
	huffman.reader_t *d = huffman.newreader(t, br);
	for (size_t i = 0; i < len; i++) {
		q[i] = huffman.read(d);
	}
	huffman.closereader(d);
	bits.closereader(br);
	reader.free(fr);
}
