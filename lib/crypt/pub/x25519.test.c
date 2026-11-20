#import crypt/pub/x25519
#import test

int main() {
	uint8_t key[32] = {};
	unhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", key);

	uint8_t zero[32] = {};
	
	x25519.key_t public_key = x25519.public_key(key);
	check("RFC 7748 Alice public", public_key.bytes, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
	
	uint8_t peer[32] = {};
	unhex("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f", peer);

	x25519.key_t shared = x25519.derive_shared_key(key, peer);
	check("RFC 7748 shared secret", shared.bytes, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");

	x25519.key_t invalid = x25519.derive_shared_key(key, zero);
	test.truth("X25519 returns zero for a low-order peer key", memcmp(invalid.bytes, zero, 32) == 0);
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