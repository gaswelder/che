#import enc/hex
#import test
#import writer

int main() {
	char out[100] = {};
	char in[] = { 0, 1, 2, 3 };

	writer.t *w = writer.static_buffer(out, 100);
	int r = hex.write(w, in, sizeof(in));
	test.truth("r==4", r == 4);
	test.streq(out, "00010203");
	return test.fails();
}
