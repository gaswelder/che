
typedef void voidfunc_t();
typedef int intfunc_t(int);

const char *S = NULL;

int main() {
    voidfunc_t *f = NULL;
    f = a; f();
	if (strcmp(S, "a") != 0) panic("!");
    f = b; f();
	if (strcmp(S, "b") != 0) panic("!");

    intfunc_t *g = NULL;
    g = dbl;
	if (g(1) != 2) panic("!");
    g = neg;
	if (g(1) != -1) panic("!");
    return 0;
}

void a() { S = "a"; }
void b() { S = "b"; }

int dbl(int x) { return 2 * x; }
int neg(int x) { return -x; }
