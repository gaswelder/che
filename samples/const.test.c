const int N = 10;

// not supported
// const int B = N * 2;

typedef {
	char name[N];
} foo_t;

int main() {
	foo_t x = {};
	if (sizeof(x.name) != 10) {
		panic("got sizeof %zu", sizeof(x.name));
	}
	// printf("B = %d\n", B);
	return 0;
}
