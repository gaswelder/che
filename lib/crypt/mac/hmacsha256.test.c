#import crypt/mac/hmacsha256
#import test

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

int main() {
	uint8_t out[32] = {};
	uint8_t key[32] = {};
	memset(key, 11, 20);
	hmacsha256.mac(key, 20, (uint8_t *) "Hi There", 8, out);
	check("RFC 4231 HMAC", out, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
	return test.fails();
}
