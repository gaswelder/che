#import lib.c

int main() {
    // Can use an imported type in a variable declaration.
    lib.foo_t val = {};
    printf("val.a = %d\n", val.a);
	printf("val.b.b = %d\n", val.b.b);
    printf("size = %zu\n", sizeof(val));

    // Can use an imported constant.
    printf("one = %d\n", lib.ONE);

    // Can call an imported function.
    lib.f();

    // Can use in a switch.
    switch (val.a) {
        case lib.ONE: {}
        default: { puts("not one"); }
    }

    f(10, val);

    // Can use ns as a variable name
    lib.foo_t lib = {};
    (&lib)->a++;
    printf("size = %zu\n", sizeof(lib));
    return 0;
}

// Can use an imported type as an argument.
void f(const int y, lib.foo_t x, ...) {
    printf("y=%d, a = %d\n", y, x.a);
}
