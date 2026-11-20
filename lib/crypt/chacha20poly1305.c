// RFC 8439. IETF ChaCha20 (96-bit nonce) and Poly1305 AEAD.

#import crypt/poly1305

pub void seal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t naad, uint8_t *p, size_t n, uint8_t tag[16]) {
	crypt(key, nonce, p, n);
	authenticate(key, nonce, aad, naad, p, n, tag);
}

// Authenticate BEFORE releasing any plaintext. p remains ciphertext on failure.
pub bool open(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t naad, uint8_t *p, size_t n, const uint8_t tag[16]) {
	uint8_t expected[16] = {};
	authenticate(key, nonce, aad, naad, p, n, expected);
	uint8_t diff = 0;
	for (int i = 0; i < 16; i++) {
		diff |= expected[i] ^ tag[i];
	}
	if (diff) {
		return false;
	}
	crypt(key, nonce, p, n);
	return true;
}

uint32_t load32(const uint8_t *p) {
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

uint32_t rotl(uint32_t x, int n) {
	return (x << n) | (x >> (32 - n));
}

void quarter(uint32_t *a, *b, *c, *d) {
	*a += *b;
	*d = rotl(*d ^ *a, 16);
	*c += *d;
	*b = rotl(*b ^ *c, 12);
	*a += *b;
	*d = rotl(*d ^ *a, 8);
	*c += *d;
	*b = rotl(*b ^ *c, 7);
}

void block(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, uint8_t out[64]) {
	uint32_t s[16] = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };
	for (int i = 0; i < 8; i++) {
		s[4 + i] = load32(key + 4 * i);
	}
	s[12] = counter;
	for (int i = 0; i < 3; i++) {
		s[13 + i] = load32(nonce + 4 * i);
	}
	uint32_t x[16] = {};
	memcpy(x, s, sizeof(s));
	for (int i = 0; i < 10; i++) {
		quarter(&x[0], &x[4], &x[8], &x[12]);
		quarter(&x[1], &x[5], &x[9], &x[13]);
		quarter(&x[2], &x[6], &x[10], &x[14]);
		quarter(&x[3], &x[7], &x[11], &x[15]);
		quarter(&x[0], &x[5], &x[10], &x[15]);
		quarter(&x[1], &x[6], &x[11], &x[12]);
		quarter(&x[2], &x[7], &x[8], &x[13]);
		quarter(&x[3], &x[4], &x[9], &x[14]);
	}
	for (int i = 0; i < 16; i++) {
		x[i] += s[i];
		for (int j = 0; j < 4; j++) {
			out[4 * i + j] = x[i] >> (8 * j);
		}
	}
}

void crypt(const uint8_t key[32], const uint8_t nonce[12], uint8_t *p, size_t n) {
	uint32_t counter = 1;
	while (n) {
		uint8_t stream[64] = {};
		block(key, nonce, counter++, stream);
		size_t k = 64;
		if (n < k) {
			k = n;
		}
		for (size_t i = 0; i < k; i++) {
			p[i] ^= stream[i];
		}
		p += k;
		n -= k;
	}
}

void padded(poly1305.t *s, const uint8_t *p, size_t n) {
	poly1305.update(s, p, n);
	uint8_t zeros[16] = {};
	poly1305.update(s, zeros, (16 - n % 16) % 16);
}

void authenticate(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t naad, const uint8_t *p, size_t n, uint8_t tag[16]) {
	uint8_t first[64] = {};
	uint8_t lengths[16] = {};
	block(key, nonce, 0, first);
	poly1305.t s = {};
	poly1305.init(&s, first);
	padded(&s, aad, naad);
	padded(&s, p, n);
	for (int i = 0; i < 8; i++) {
		lengths[i] = (uint64_t) naad >> (8 * i);
		lengths[8 + i] = (uint64_t) n >> (8 * i);
	}
	poly1305.update(&s, lengths, 16);
	poly1305.end(&s, tag);
}
