// HMAC-SHA-256 (RFC 2104/4231).
#import crypt/hash/sha256

pub void mac(const uint8_t *key, size_t keylen, const uint8_t *msg, size_t len, uint8_t out[32]) {
	uint8_t k[64] = {};
	uint8_t inner[32] = {};
	if (keylen > 64) {
		sha256.hash(key, keylen, k);
	} else {
		memcpy(k, key, keylen);
	}
	for (int i = 0; i < 64; i++) {
		k[i] ^= 0x36;
	}
	sha256.t s = {};
	sha256.init(&s);
	sha256.update(&s, k, 64);
	sha256.update(&s, msg, len);
	sha256.digest(&s, inner);
	for (int i = 0; i < 64; i++) {
		k[i] ^= 0x36 ^ 0x5c;
	}
	sha256.init(&s);
	sha256.update(&s, k, 64);
	sha256.update(&s, inner, 32);
	sha256.digest(&s, out);
	memset(k, 0, sizeof(k));
}
