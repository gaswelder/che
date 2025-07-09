
pub typedef {
    int value;
} mpz_t;

pub typedef {
    double value;
} mpf_t;

pub void print(const char *f, void *x) {
	if (strcmp(f, "%Zd") == 0) {
		mpz_t *n = x;
		printf("%d", n->value);
	} else {
		mpf_t *n = x;
		printf("%g", n->value);
	}
}

pub void mpf_clear(mpf_t *x) { puts("todo mpf_clear"); (void) x; }
pub void mpz_clear(mpz_t *x) { puts("todo mpz_clear"); (void) x; }

pub void mpf_add(mpf_t *a, *b, *c) { a->value = b->value + c->value; }
pub void mpz_add(mpz_t *a, *b, *c) { a->value = b->value + c->value; }

pub void mpf_div(mpf_t *a, *b, *c) { puts("todo mpf_div"); (void) a; (void) b; (void) c; }
pub void mpz_div(mpz_t *a, *b, *c) { puts("todo mpz_div"); (void) a; (void) b; (void) c; }

pub void mpf_mul(mpf_t *a, *b, *c) { puts("todo mpf_mul"); (void) a; (void) b; (void) c; }
pub void mpz_mul(mpz_t *a, *b, *c) { puts("todo mpz_mul"); (void) a; (void) b; (void) c; }

pub void mpz_sub(mpz_t *a, *b, *c) { a->value = b->value - c->value; }
pub void mpf_sub(mpf_t *a, *b, *c) { a->value = b->value - c->value; }

pub int mpf_cmp(mpf_t *a, *b) { puts("todo mpf_cmp"); (void) a; (void) b; return 0; }
pub int mpz_cmp(mpz_t *a, *b) { puts("todo mpz_cmp"); (void) a; (void) b; return 0; }

pub void mpf_neg(mpf_t *a, *b) { a->value = -b->value; }
pub void mpz_neg(mpz_t *a, *b) { a->value = -b->value; }


pub double mpf_get_d(mpf_t *a) { puts("todo mpf_get_d"); (void) a; return 0; }

pub void mpf_set_d(mpf_t *a, double b) { 
	a->value = b;
}

pub int mpz_get_si(mpz_t *a) { puts("todo mpz_get_si"); (void) a; return 0; }

pub void *mpz_get_str(void *a, int b, mpz_t *c) { puts("todo mpz_get_str"); (void) a; (void) b; (void) c; return NULL; }

pub void mpf_set_str(mpf_t *a, const char *b) {
	sscanf(b, "%lf", &a->value);
}
pub void mpf_set_z(mpf_t *a, mpz_t *b) {
	a->value = (double) b->value;
}
pub void mpf_set(mpf_t *a, *b) {
	printf("todo mpf_set\n");
	(void) a; (void) b;}

pub void mpz_mod(mpz_t *a, *b, *c) {
	printf("todo mpz_mod\n");
	(void) a; (void) b; (void) c; }

pub void mpz_set_str(mpz_t *z, const char *s) {
	sscanf(s, "%d", &z->value);
}
pub void mpz_set_ui(mpz_t *z, int n) {
	z->value = n;
}

pub void mpz_set(mpz_t *a, *b) {
	a->value = b->value;
}

pub void *mpf_get_str(void *a, *b, int c, void *d, mpf_t *e) {
	printf("todo get_str\n");
    (void) a;
    (void) b;
    (void) c;
    (void) d;
    (void) e;
    return NULL;
}

