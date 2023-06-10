#import lib.c

int main() {
    // Can use an imported type in a variable declaration.
    lib.foo_t f = {};
    printf("size = %zu\n", sizeof(f));

    // Can use an imported constant.
    printf("one = %d\b", lib.ONE);

    // Can call an imported function.
    lib.f();
    return 0;
}
