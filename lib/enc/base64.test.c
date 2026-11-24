#import writer
#import reader
#import enc/base64
#import test

int main() {
	BASE64("", "");
	BASE64("f", "Zg==");
	BASE64("fo", "Zm8=");
	BASE64("foo", "Zm9v");
	BASE64("foob", "Zm9vYg==");
	BASE64("fooba", "Zm9vYmE=");
	BASE64("foobar", "Zm9vYmFy");
	BASE64("f", "Zg==");

	uint8_t out[3] = {};
	uint8_t expected[3] = {0, 255, 128};
	if (decode("AP+A", out) != sizeof(expected) ||
		memcmp(out, expected, sizeof(expected)) != 0) {
		panic("binary decode failed");
	}
	return test.fails();
}

void BASE64(const char *input, *encoded) {
	uint8_t out[100] = {};

	encode(input, (uint8_t *) out, 100);
	test.streq((char *) out, encoded);

	memset(out, 0, 100);

	decode(encoded, out);
	test.streq((char *) out, input);
}

void encode(const char *input, uint8_t *buf, size_t bufsize) {
	writer.t *w = writer.static_buffer(buf, bufsize);
	base64.encoder_t *wrt = base64.newencoder(w);
	base64.encode(wrt, (uint8_t *) input, strlen(input));
	base64.closeencoder(wrt);
	writer.free(w);
}

size_t decode(const char *input, uint8_t *buf) {
	reader.t *r = reader.string(input);
	base64.decoder_t *d = base64.newdecoder(r);
	int res = base64.decode(d, buf, 100);
	base64.freedecoder(d);
	reader.free(r);
	return (size_t) res;
}
