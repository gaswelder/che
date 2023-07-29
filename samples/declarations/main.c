#define A 10
#define B 20
#define C 30

// Counts as usage of A.
int a[A] = {};

typedef {
    // Counts as usage of C.
    int c[C];
} c_t;

int main() {
    printf("%zu\n", nelem(a));

    // Counts as usage of B.
    int b[B] = {};    
    printf("%zu\n", nelem(b));

    c_t c = {};
    printf("%zu\n", nelem(c.c));

    return 0;
}
