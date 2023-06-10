#import lib.c

int main() {
    lib.foo_t f = {};
    printf("size = %zu\n", sizeof(f));
    lib.f();
    return 0;
}
