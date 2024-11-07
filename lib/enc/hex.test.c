#import enc/hex
#import test
#import writer

int main() {
	uint8_t out[100] = {};
	char in[] = { 0, 1, 2, 3 };

	writer.t *w = writer.static_buffer(out, 100);
	int r = hex.write(w, in, sizeof(in));
	test.truth("r==4", r == 4);
	test.streq((char *)out, "00010203");
	return test.fails();
}
