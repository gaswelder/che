pub typedef {
    int x;
} mpz_t;

pub typedef {
    double x;
} mpf_t;



pub void gmp_printf(const char *f, void *x) {
    (void) f;
    (void) x;
}

pub void mpz_init(mpz_t z) { (void) z; }

pub void mpf_clear(mpf_t x) { (void) x; }
pub void mpz_clear(mpz_t x) { (void) x; }

pub void *mpf_add(mpf_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }
pub void *mpf_div(mpf_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }
pub void *mpf_mul(mpf_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }
pub void *mpf_sub(mpf_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }
pub void *mpz_add(mpz_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }
pub void *mpz_div(mpz_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }
pub void *mpz_mul(mpz_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }
pub void *mpz_sub(mpz_t a, b, c) { (void) a; (void) b; (void) c; return NULL; }

pub int mpf_cmp(mpf_t a, b) { (void) a; (void) b; return 0; }
pub int mpz_cmp(mpz_t a, b) { (void) a; (void) b; return 0; }

pub void mpf_neg(mpf_t a, b) { (void) a; (void) b; }
pub void mpz_neg(mpz_t a, b) { (void) a; (void) b; }



pub double mpf_get_d(mpf_t a) { (void) a; return 0; }
pub double mpf_set_d(mpf_t a, double b) { (void) a; (void) b; return 0; }

pub int mpz_get_si(mpz_t a) { (void) a; return 0; }

pub void *mpz_get_str(void *a, int b, mpz_t c) { (void) a; (void) b; (void) c; return NULL; }

pub void mpf_init2(mpf_t a, int b) { (void) a; (void) b; }

pub void mpf_set_str(mpf_t a, void *b, int c) { (void) a; (void) b; (void) c; }
pub void mpf_set_z(mpf_t a, mpz_t b) { (void) a; (void) b;}
pub void mpf_set(mpf_t a, b) { (void) a; (void) b;}

pub void mpz_mod(mpz_t a, b, c) { (void) a; (void) b; (void) c; }

pub void mpz_set_str(mpz_t z, void *nstr, int ten) { (void) z; (void) nstr; (void) ten; }
pub void mpz_set_ui(mpz_t z, int n) { (void) z; (void) n; }
pub void mpz_set(mpz_t a, b) { (void) a;  (void) b;}

pub void *mpf_get_str(void *a, *b, int c, void *d, mpf_t e) {
    (void) a;
    (void) b;
    (void) c;
    (void) d;
    (void) e;
    return NULL;
}

