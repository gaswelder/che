// Fills the buffer with random bytes from the OS source.
// Returns false on failure.
pub bool bytes(uint8_t *p, size_t n) {
	FILE *f = fopen("/dev/urandom", "rb");
	if (!f) {
		panic("failed to open /dev/urandom");
	}
	size_t got = fread(p, 1, n, f);
	bool ok = got == n && !ferror(f);
	if (!ok) {
		panic("failed to read /dev/urandom");
	}
	fclose(f);
	return ok;
}

// Returns a random integer in the range [0, n).
// Panics if n = 0 or the random source fails.
pub uint32_t intn(uint32_t n) {
	if (n == 0) {
		panic("got n = 0");
	}
	// Discard the incomplete group so every remainder is equally likely.
	uint32_t threshold = 0 - n;
	threshold %= n;
	while (true) {
		uint32_t value = 0;
		if (!bytes((uint8_t *)&value, sizeof(value))) {
			panic("OS random source failed");
		}
		if (value >= threshold) {
			return value % n;
		}
	}
}

// Returns a random number uniformly distributed in [0, 1).
// Panics if the OS random source fails.
pub double u() {
	uint64_t value = 0;
	if (!bytes((uint8_t *)&value, sizeof(value))) {
		panic("OS random source failed");
	}
	// Binary64 can represent every 53-bit integer exactly.
	return (double)(value >> 11) / 9007199254740992.0;
}
