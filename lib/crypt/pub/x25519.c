// RFC 7748

pub typedef {
	uint8_t bytes[32];
} key_t;

typedef {
	uint64_t v[17];
} field_t;

// Returns the public key for the given private key.
pub key_t public_key(const uint8_t private_key[32]) {
	uint8_t base[32] = { 9 };
	return derive_shared_key(private_key, base);
}

// Returns the shared key for Alice's private key and Bob's public key.
pub key_t derive_shared_key(const uint8_t private_key[32], const uint8_t peer_public_key[32]) {
	key_t key = {};
	uint8_t k[32] = {};
	memcpy(k, private_key, 32);
	k[0] &= 248;
	k[31] = (k[31] & 127) | 64;
	field_t x1 = {};
	field_t x2 = {};
	field_t z2 = {};
	field_t x3 = {};
	field_t z3 = {};
	field_t c = {};
	for (int i = 0; i < 255; i++) {
		x1.v[i / 15] |= (uint64_t)((peer_public_key[i / 8] >> (i % 8)) & 1) << (i % 15);
	}
	x2.v[0] = 1;
	x3 = x1;
	z3.v[0] = 1;
	c.v[0] = 121665;
	carry(&c);
	uint64_t last = 0;
	for (int i = 254; i >= 0; i--) {
		uint64_t bit = (k[i / 8] >> (i % 8)) & 1;
		swap(&x2, &x3, last ^ bit);
		swap(&z2, &z3, last ^ bit);
		last = bit;
		field_t a = {};
		field_t aa = {};
		field_t b = {};
		field_t bb = {};
		field_t e = {};
		field_t d = {};
		field_t da = {};
		field_t cb = {};
		field_t t = {};
		add(&a, &x2, &z2);
		mul(&aa, &a, &a);
		sub(&b, &x2, &z2);
		mul(&bb, &b, &b);
		sub(&e, &aa, &bb);
		add(&t, &x3, &z3);
		mul(&cb, &t, &b);
		sub(&d, &x3, &z3);
		mul(&da, &d, &a);
		add(&t, &da, &cb);
		mul(&x3, &t, &t);
		sub(&t, &da, &cb);
		mul(&t, &t, &t);
		mul(&z3, &x1, &t);
		mul(&x2, &aa, &bb);
		mul(&t, &c, &e);
		add(&t, &aa, &t);
		mul(&z2, &e, &t);
	}
	swap(&x2, &x3, last);
	swap(&z2, &z3, last);
	field_t inverse = {};
	inverse.v[0] = 1;
	for (int i = 254; i >= 0; i--) {
		mul(&inverse, &inverse, &inverse);
		if (i != 2 && i != 4) {
			mul(&inverse, &inverse, &z2);
		}
	}
	mul(&x2, &x2, &inverse);
	// Canonical reduction by p, using a borrow and a conditional selection.
	field_t reduced = {};
	uint64_t borrow = 0;
	for (int i = 0; i < 17; i++) {
		uint64_t prime = 32767;
		if (i == 0) {
			prime -= 18;
		}
		uint64_t t = x2.v[i] - prime - borrow;
		reduced.v[i] = t & 32767;
		borrow = t >> 63;
	}
	swap(&x2, &reduced, borrow ^ 1);
	for (int i = 0; i < 255; i++) {
		key.bytes[i / 8] |= ((x2.v[i / 15] >> (i % 15)) & 1) << (i % 8);
	}
	uint8_t *wipe = k;
	for (int i = 0; i < 32; i++) {
		wipe[i] = 0;
	}
	return key;
}

void carry(field_t *a) {
	for (int pass = 0; pass < 3; pass++) {
		for (int i = 0; i < 16; i++) {
			a->v[i + 1] += a->v[i] >> 15;
			a->v[i] &= 32767;
		}
		uint64_t high = a->v[16] >> 15;
		a->v[16] &= 32767;
		a->v[0] += 19 * high;
	}
}

void add(field_t *r, const field_t *a, *b) {
	for (int i = 0; i < 17; i++) {
		r->v[i] = a->v[i] + b->v[i];
	}
	carry(r);
}

void sub(field_t *r, const field_t *a, *b) {
	for (int i = 0; i < 17; i++) {
		r->v[i] = a->v[i] + 65534 - b->v[i];
	}
	r->v[0] -= 36;
	carry(r);
}

void mul(field_t *r, const field_t *a, *b) {
	uint64_t t[34] = {};
	for (int i = 0; i < 17; i++) {
		for (int j = 0; j < 17; j++) {
			t[i + j] += a->v[i] * b->v[j];
		}
	}
	for (int i = 17; i < 33; i++) {
		t[i - 17] += 19 * t[i];
	}
	for (int i = 0; i < 17; i++) {
		r->v[i] = t[i];
	}
	carry(r);
}

void swap(field_t *a, *b, uint64_t bit) {
	uint64_t mask = 0 - bit;
	for (int i = 0; i < 17; i++) {
		uint64_t t = mask & (a->v[i] ^ b->v[i]);
		a->v[i] ^= t;
		b->v[i] ^= t;
	}
}
