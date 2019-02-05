
typedef {
	int a;
	int c;
	int m;
	int x;
} lcg_t;

lcg_t s = {445, 700001, 2097152, 0};

pub void lcg_seed(int a, int c, int m, int x) {
	s.a = a;
	s.c = c;
	s.m = m;
	s.x = x;
}

pub int lcg_rand() {
	assert(s.x < INT_MAX / s.a);
	s.x = (s.x * s.a + s.c) % s.m;
	return s.x;
}
