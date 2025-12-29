#import compress/huffman
#import test
#import bits

pub int main() {
	const char *s = "The quick brown fox jumps over the lazy dog.";
	huffman.tree_t *t = huffman.buildtree(s);

	// Write the message
	FILE *f = tmpfile();
	huffman.writer_t *w = huffman.newwriter(t, f);
	const char *p;
	for (p = s; *p != '\0'; p++) {
		uint8_t x = *p;
		huffman.write(w, x);
	}
	huffman.closewriter(w);

	// Read the message back
	fseek(f, 0, SEEK_SET);
	huffman.reader_t *d = huffman.newreader(t, f);
	size_t len = strlen(s);
	char *q = calloc!(len+1, 1);
	for (size_t i = 0; i < len; i++) {
		q[i] = huffman.read(d);
	}
	test.streq(s, q);
	huffman.closereader(d);
	fclose(f);

	huffman.freetree(t);

	testcanonical();

	return test.fails();
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
	bits.writer_t *w = bits.newwriter(f);
	bits.write(w, bits, nelem(bits));
	bits.closewriter(w);

	// Recreate the tree from the spec
	huffman.tree_t *t = huffman.treefrom(lencounts, nelem(lencounts), (uint8_t *) characters);

	// Read the message back
	char out[13] = {};
	rewind(f);
	huffman.reader_t *r = huffman.newreader(t, f);
	for (int i = 0; i < 12; i++) {
		out[i] = huffman.read(r);
	}
	huffman.closereader(r);
	fclose(f);

	test.streq(msg, out);
}
