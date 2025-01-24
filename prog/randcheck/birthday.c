#import rnd

// This checks the quality of the random number generator by simulating the
// Birtday Paradox case and comparing the observed duplicates count with the
// predicted count.

// The problem deals with 365 days, and here "days" are the integers that the
// generator can possibly yield. This is a toy, so we won't scan the whole
// uint32 space and choose a lower bound and use rnd.intn.
// 1000000 easily fails the current rnd.intn.
const int MAX = 1000000;

pub int main() {
    double desired = 0.01;
	double factor = sqrt(-2.0*log(desired));
    double rng_range = 1.0 + MAX;
    size_t sample_size = ceil(factor * sqrt(rng_range));
    double expected = sample_size - rng_range * (- OS.expm1(sample_size*OS.log1p(-1/rng_range)));
    float p_zero = exp(-expected);

    uint32_t *values = calloc(sample_size, sizeof(uint32_t));
    for (size_t i = 0; i < sample_size; ++i) {
        values[i] = (int) rnd.intn(MAX);
	}

    size_t repeats = count_repeats(values, sample_size);
	printf("size = %zu\n", sample_size);
    printf("repeats = %zu, expected %f, p(zero repeats) = %f\n", repeats, expected, p_zero);

    double p_value = 0;
    for (size_t k = 0; k <= repeats; ++k) {
        double pdf_value = exp(log(expected)*k - expected - OS.lgamma(1.0+k));
        p_value += pdf_value;
        if (p_value > 0.5) {
			p_value = p_value - 1;
		}
    }
	printf("p-value = %f\n", p_value);
    return 0;
}

int cmpu32(const void *a, *b) {
	const uint32_t *aa = a;
	const uint32_t *bb = b;
	return *aa - *bb;
}

size_t count_repeats(uint32_t *values, size_t n) {
	qsort(values, n, sizeof(uint32_t), cmpu32);
    uint32_t prev = values[0];
    size_t count = 0;
    for (size_t i = 1; i < n; ++i) {
        uint32_t curr = values[i];
        if (prev == curr) {
            count++;
        }
        prev = curr;
    }
    return count;
}
