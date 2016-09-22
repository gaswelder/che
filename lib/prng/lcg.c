
struct __state {
	int a;
	int c;
	int m;
	int x;
};

static struct __state s;

void lcg_seed(int a, int c, int m, int x) {
	s.a = a;
	s.c = c;
	s.m = m;
	s.x = x;
}

int lcg_rand() {
	assert(s.x < INT_MAX / s.a);
	s.x = (s.x * s.a + s.c) % s.m;
	return s.x;
}
