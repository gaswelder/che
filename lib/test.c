int _fails = 0;

pub bool streq(const char *a, *b) {
	if (!a && !b) {
		return true;
	}
	if (!a || !b || strcmp(a, b) != 0) {
		printf("FAIL: string(%s) != string(%s)\n", a, b);
		_fails++;
		return false;
	}
	return true;
}

pub int fails() {
	return _fails;
}

pub bool truth(const char *comment, bool result) {
	if (!result) {
		printf("FAIL: %s\n", comment);
		_fails++;
	}
	return result;
}

// Allocates a buffer of size n. n=0 is allowed.
uint8_t *sample_alloc(size_t n) {
	size_t buf = n;
	if (!buf) {
		buf = 1;
	}
	return calloc!(1, buf);
}

// A run of zero bytes.
pub uint8_t *sample_zeros(size_t n) {
	uint8_t *p = sample_alloc(n);
	for (size_t i = 0; i < n; i++) {
		p[i] = 0;
	}
	return p;
}

// A simple arithmetic pattern.
pub uint8_t *sample_pattern(size_t n) {
	uint8_t *p = sample_alloc(n);
	for (size_t i = 0; i < n; i++) {
		p[i] = (uint8_t)(i * 7 + i / 251);
	}
	return p;
}

// Pseudorandom bytes from a fixed seed.
pub uint8_t *sample_random(size_t n) {
	uint8_t *p = sample_alloc(n);
	uint32_t r = 0x12345678;
	for (size_t i = 0; i < n; i++) {
		r = r * 1664525 + 1013904223;
		p[i] = (uint8_t)(r >> 24);
	}
	return p;
}

// Repeating text.
pub uint8_t *sample_text(size_t n) {
	uint8_t *p = sample_alloc(n);
	const char *text = "The quick brown fox jumps over the lazy dog. ";
	size_t tlen = 45;
	for (size_t i = 0; i < n; i++) {
		p[i] = (uint8_t) text[i % tlen];
	}
	return p;
}

// Compressible text runs with zero and random islands.
pub uint8_t *sample_mixed(size_t n) {
	uint8_t *p = sample_alloc(n);
	uint32_t r = 0x12345678;
	const char *text = "The quick brown fox jumps over the lazy dog. ";
	size_t tlen = 45;
	for (size_t i = 0; i < n; i++) {
		switch ((i / 1000) % 3) {
			case 0: { p[i] = (uint8_t) text[i % tlen]; }
			case 1: { p[i] = 0; }
			default: {
				r = r * 1664525 + 1013904223;
				p[i] = (uint8_t)(r >> 24);
			}
		}
	}
	return p;
}
