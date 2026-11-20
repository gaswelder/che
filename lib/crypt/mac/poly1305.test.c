#import crypt/mac/poly1305
#import test

int main() {
	uint8_t key[32] = {};
	uint8_t out[16] = {};
	unhex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b", key);
	poly1305.mac(key, (uint8_t *) "Cryptographic Forum Research Group", 34, out);
	check("RFC 8439 Poly1305", out, "a8061dc1305136c6c22b8baf0c0127a9");
	const uint8_t *msg = (uint8_t *) "Cryptographic Forum Research Group";
	for (size_t split = 0; split <= 34; split++) {
		poly1305.t state = {};
		poly1305.init(&state, key);
		poly1305.update(&state, msg, split);
		poly1305.update(&state, msg + split, 34 - split);
		poly1305.end(&state, out);
		check("Poly1305 split updates", out, "a8061dc1305136c6c22b8baf0c0127a9");
	}
	poly1305.mac(key, NULL, 0, out);
	test.truth("Poly1305 empty message", memcmp(out, key + 16, 16) == 0);
	return test.fails();
}

void unhex(const char *s, uint8_t *out) {
	while (*s != '\0') {
		uint32_t v = 0;
		sscanf(s, "%2x", &v);
		*out++ = v;
		s += 2;
	}
}

void check(const char *name, const uint8_t *got, const char *hex) {
	uint8_t want[256] = {};
	unhex(hex, want);
	test.truth(name, memcmp(got, want, strlen(hex) / 2) == 0);
}
