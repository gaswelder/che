#import enc/hex
#import reader
#import test
#import writer

int main() {
	enc();
	dec();
	return test.fails();
}

void enc() {
	uint8_t out[100] = {};
	char in[] = { 0, 1, 2, 3 };

	writer.t *w = writer.static_buffer(out, 100);
	int r = hex.write(w, in, sizeof(in));
	test.truth("r==4", r == 4);
	test.streq((char *) out, "00010203");
}

void dec() {
	uint8_t out[4] = {};

	reader.t *sr = reader.string("00010203");
	for (int i = 0; i < 4; i++) {
		out[i] = hex.read(sr);
	}
	reader.free(sr);
	for (int i = 0; i < 4; i++) {
		test.truth("byte comparison", out[i] == (uint8_t) i);
	}
}