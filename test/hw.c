import "prng/neumann"

int main() {
	int16_t n = (int16_t)(time(NULL));

	int i;
	for(i = 0; i < 100; i++) {
		n = nrg_next(n);
		printf("%d\n", n);
	}
}
