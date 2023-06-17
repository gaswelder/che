pub enum {
    ONE = 1,
    TWO = 2
};
pub typedef { int a; } foo_t;
pub void f() {
    foo_t x = {};
    printf("x.a = %d\n", x.a);
    puts("OK");
}
