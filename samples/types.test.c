
typedef void voidfunc_t();
typedef int intfunc_t(int);


int main() {
    voidfunc_t *f = NULL;
    f = a; f();
    f = b; f();

    intfunc_t *g = NULL;
    g = dbl; printf("%d\n", g(1));
    g = neg; printf("%d\n", g(1));

    return 0;
}

void a() { puts("a"); }
void b() { puts("b"); }

int dbl(int x) { return 2 * x; }
int neg(int x) { return -x; }
