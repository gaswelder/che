#import compress/huffman
#import test

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

	return test.fails();
}
