#include <unistd.h>

const int n = 10;

bool _seeded = false;
uint64_t pcg32_value = 0;
uint64_t pcg32_m = 0x9b60933458e17d7d;
uint64_t pcg32_a = 0xd737232eeccdf7ed;

uint32_t pcg32() {
	if (!_seeded) {
		uint64_t x = time(NULL);
		x += (uint64_t) OS.getpid();
		seed(x);
	}
    pcg32_value = pcg32_value * pcg32_m + pcg32_a;
    int shift = 29 - (pcg32_value >> 61);
    return pcg32_value >> shift;
}

// Seeds the generator's state.
// Use this for reproducible runs.
// If not called, a seed will be chosen automatically.
pub void seed(uint64_t s) {
    pcg32_value = s;
	_seeded = true;
}

// Returns a random integer in the range [0, n).
// Panics if n = 0.
pub uint32_t intn(uint32_t n) {
	if (n == 0) {
		panic("got n = 0 in rand.intn");
	}
	// There's a bias here, but it'll do for now.
	return pcg32() % n;
}

pub double gauss() {
    // Brute-force approach until something better comes.
    double v = u();
    for (int j = 0; j < n; j++) {
        v += u();
    }
    return v / (double) (n+1) - 0.5;
}


pub double exponential(double mean) {
    return mean * -log(u());
}

// Returns a random double uniformly distributed in [0, 1).
pub double u() {
    return (double) pcg32() / 0xffffffff;
}

// Returns a random double uniformly distributed in [low, high).
pub double urange(double low, high) {
    return low + (high-low) * u();
}
