#import namespaceslib.c

int main() {
    // Can use an imported type in a variable declaration.
    namespaceslib.foo_t val = {};
    printf("val.a = %d\n", val.a);
	printf("val.b.b = %d\n", val.b.b);
    printf("size = %zu\n", sizeof(val));

    // Can use an imported constant.
    printf("one = %d\n", namespaceslib.ONE);

    // Can call an imported function.
    namespaceslib.f();

    // Can use in a switch.
    switch (val.a) {
        case namespaceslib.ONE: {}
        default: { puts("not one"); }
    }

    f(10, val);

    // Can use ns as a variable name
    namespaceslib.foo_t namespaceslib = {};
    (&namespaceslib)->a++;
    printf("size = %zu\n", sizeof(namespaceslib));
    return 0;
}

// Can use an imported type as an argument.
void f(const int y, namespaceslib.foo_t x, ...) {
    printf("y=%d, a = %d\n", y, x.a);
}
