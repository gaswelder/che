#import mem
#import test

int main() {
	mem.mem_t *m = mem.memopen();
	if (!m) {
		panic("memopen failed");
	}

	// Fill the buffer with some data.
	const char *sample = "012345678";
	size_t len = strlen(sample);
	size_t r = mem.memwrite(m, sample, len);
	if (r < len) {
		panic("memwrite failed");
	}

	// Rewind the buffer and check the contents.
	const char *p = sample;
	mem.rewind(m);
	while (true) {
		int c = mem.memgetc(m);
		if (c == EOF) {
			break;
		}
		test.truth("memgetc == *p", c == *p++);
	}
	mem.memclose(m);
	return 0;
}
