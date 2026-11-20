// RFC 8439 Poly1305 message authentication.

pub typedef {
	uint64_t h[10];
	uint64_t r[10];
	uint8_t s[16];
	uint8_t buffer[16];
	size_t used;
} t;

// Computes the MAC of msg using key, writes out into tag.
pub void mac(const uint8_t key[32], const uint8_t *msg, size_t len, uint8_t tag[16]) {
	t s = {};
	init(&s, key);
	update(&s, msg, len);
	end(&s, tag);
}


// Initializes state s with the given key.
pub void init(t *s, const uint8_t key[32]) {
	memset(s, 0, sizeof(t));
	uint8_t r[16] = {};
	memcpy(r, key, 16);
	r[3] &= 15;
	r[7] &= 15;
	r[11] &= 15;
	r[15] &= 15;
	r[4] &= 252;
	r[8] &= 252;
	r[12] &= 252;
	for (int i = 0; i < 128; i++) {
		s->r[i / 13] |= (uint64_t)((r[i / 8] >> (i % 8)) & 1) << (i % 13);
	}
	memcpy(s->s, key + 16, 16);
}

// Adds message bytes to the hash.
pub void update(t *s, const uint8_t *msg, size_t len) {
	while (len > 0) {
		if (s->used == 0 && len >= 16) {
			block(s, msg, 16);
			msg += 16;
			len -= 16;
			continue;
		}
		size_t k = 16 - s->used;
		if (k > len) k = len;
		memcpy(s->buffer + s->used, msg, k);
		s->used += k;
		msg += k;
		len -= k;
		if (s->used == 16) {
			block(s, s->buffer, 16);
			s->used = 0;
		}
	}
}

// Ends the input and writes out the hash result into tag.
pub void end(t *s, uint8_t tag[16]) {
	if (s->used > 0) block(s, s->buffer, s->used);
	uint64_t reduced[10] = {};
	uint64_t borrow = 0;
	for (int i = 0; i < 10; i++) {
		uint64_t prime = 8191;
		if (i == 0) {
			prime -= 4;
		}
		uint64_t v = s->h[i] - prime - borrow;
		reduced[i] = v & 8191;
		borrow = v >> 63;
	}
	uint64_t mask = 0 - (borrow ^ 1);
	for (int i = 0; i < 10; i++) {
		s->h[i] ^= mask & (s->h[i] ^ reduced[i]);
	}
	memset(tag, 0, 16);
	for (int i = 0; i < 128; i++) {
		tag[i / 8] |= ((s->h[i / 13] >> (i % 13)) & 1) << (i % 8);
	}
	uint32_t c = 0;
	for (int i = 0; i < 16; i++) {
		c += tag[i] + s->s[i];
		tag[i] = c;
		c = c >> 8;
	}
}

void carry(uint64_t h[10]) {
	for (int pass = 0; pass < 3; pass++) {
		for (int i = 0; i < 9; i++) {
			h[i + 1] += h[i] >> 13;
			h[i] &= 8191;
		}
		uint64_t high = h[9] >> 13;
		h[9] &= 8191;
		h[0] += high * 5;
	}
}

void block(t *s, const uint8_t *msg, size_t len) {
	for (size_t i = 0; i < len * 8; i++) {
		s->h[i / 13] += (uint64_t)((msg[i / 8] >> (i % 8)) & 1) << (i % 13);
	}
	s->h[len * 8 / 13] += (uint64_t) 1 << ((len * 8) % 13);
	uint64_t products[20] = {};
	for (int i = 0; i < 10; i++) {
		for (int j = 0; j < 10; j++) {
			products[i + j] += s->h[i] * s->r[j];
		}
	}
	for (int i = 10; i < 19; i++) {
		products[i - 10] += 5 * products[i];
	}
	for (int i = 0; i < 10; i++) {
		s->h[i] = products[i];
	}
	carry(s->h);
}
