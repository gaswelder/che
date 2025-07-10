
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

pub void mpz_sprint(char *buf, mpz_t *c) { sprintf(buf, "%x", c->value); }
pub void mpf_sprint(char *buf, mpf_t *c) { sprintf(buf, "%f", c->value); }

pub void mpf_add(mpf_t *a, *b, *c) { a->value = b->value + c->value; }
pub void mpz_add(mpz_t *a, *b, *c) { a->value = b->value + c->value; }

pub void mpf_div(mpf_t *a, *b, *c) { a->value = b->value / c->value; }
pub void mpz_div(mpz_t *a, *b, *c) { a->value = b->value / c->value; }

pub void mpf_mul(mpf_t *a, *b, *c) { a->value = b->value * c->value; }
pub void mpz_mul(mpz_t *a, *b, *c) { a->value = b->value * c->value; }

pub void mpz_sub(mpz_t *a, *b, *c) { a->value = b->value - c->value; }
pub void mpf_sub(mpf_t *a, *b, *c) { a->value = b->value - c->value; }

pub void mpf_neg(mpf_t *a, *b) { a->value = -b->value; }
pub void mpz_neg(mpz_t *a, *b) { a->value = -b->value; }

pub void mpz_set(mpz_t *a, *b) { a->value = b->value; }
pub void mpf_set(mpf_t *a, *b) { a->value = b->value; }

pub void mpf_set_str(mpf_t *a, const char *b) { sscanf(b, "%lf", &a->value); }
pub void mpz_set_str(mpz_t *z, const char *s) { sscanf(s, "%d", &z->value); }

pub void mpf_set_d(mpf_t *a, double b) { a->value = b; }
pub void mpz_set_ui(mpz_t *z, int n) { z->value = n; }

pub double mpf_get_d(mpf_t *a) { return a->value; }
pub int mpz_get_si(mpz_t *a) { return a->value; }

pub void mpz_mod(mpz_t *a, *b, *c) { a->value = b->value % c->value; }

pub int mpf_cmp(mpf_t *a, *b) {
	if (a->value > b->value) return 1;
	if (a->value < b->value) return -1;
	return 0;
}
pub int mpz_cmp(mpz_t *a, *b) {
	if (a->value > b->value) return 1;
	if (a->value < b->value) return -1;
	return 0;
}


pub void mpf_set_z(mpf_t *a, mpz_t *b) { a->value = (double) b->value; }

