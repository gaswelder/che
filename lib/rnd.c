const int n = 10;

// Returns a random number in the range [min, max).
// Panics if min >= max.
pub uint32_t range(uint32_t min, max) {
	if (min >= max) {
		panic("expected min < max, got min=%d, max=%d", min, max);
	}
	uint32_t size = max - min;
	// There's a bias here, but it'll do for now.
	return min + pcg32() % size;
}

// Returns a random integer in the range [0, n).
// Panics if n = 0.
pub int intn(int n) {
	if (n == 0) {
		panic("got n = 0");
	}
	return (rand() & 0x7FFF) % n;
}

pub double gauss() {
    // Brute-force approach until something better comes.
    double v = (double)rand() / RAND_MAX;
    for (int j = 0; j < n; j++) {
        v += (double)rand() / RAND_MAX;
    }
    return v / (n+1) - 0.5;
}

pub double uniform(double low, high) {
    return low + (high-low) * u();
}

pub double exponential(double mean) {
    return mean * -log(u());
}

double u() {
    return (double) pcg32() / 0xffffffff;
}

pub uint32_t u32() {
    return pcg32();
}

uint64_t pcg32_value = 0;
uint64_t pcg32_m = 0x9b60933458e17d7d;
uint64_t pcg32_a = 0xd737232eeccdf7ed;

pub void seed(uint64_t s) {
    pcg32_value = s;
}

uint32_t pcg32() {
    pcg32_value = pcg32_value * pcg32_m + pcg32_a;
    int shift = 29 - (pcg32_value >> 61);
    return pcg32_value >> shift;
}
