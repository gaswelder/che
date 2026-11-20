#import crypt/hash/sha256
#import test

int main() {
	uint8_t out[32] = {};
	sha256.hash((uint8_t *) "", 0, out);
	check("SHA256 empty", out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
	sha256.hash((uint8_t *) "abc", 3, out);
	check("SHA256 abc", out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	sha256.t hash = {};
	sha256.init(&hash);
	uint8_t a = 'a';
	for (int i = 0; i < 1000000; i++) {
		sha256.update(&hash, &a, 1);
	}
	sha256.digest(&hash, out);
	check("SHA256 million a", out, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
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
