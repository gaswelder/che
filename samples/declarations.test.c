#define A 10
#define B 20
#define C 30

int a[A] = {}; // Counts as usage of A.

typedef {
	int c[C]; // Counts as usage of C.
} c_t;

int main() {
	int b[B] = {}; // Counts as usage of B.
	c_t c = {};

	if (nelem(a) != 10) panic("!");
	if (nelem(b) != 20) panic("!");
	if (nelem(c.c) != 30) panic("!");
	return 0;
}
