#import lib.c

int main() {
    // Can use an imported type in a variable declaration.
    lib.foo_t val = {};
    printf("size = %zu\n", sizeof(val));

    // Can use an imported constant.
    printf("one = %d\b", lib.ONE);

    // Can call an imported function.
    lib.f();

    // Can use in a switch.
    switch (val.a) {
        case lib.ONE:
            break;
        default:
            puts("not one");
    }

    f(val);
    return 0;
}

// Can use an imported type as an argument.
void f(lib.foo_t x) {
    printf("a = %d\n", x.a);
}
