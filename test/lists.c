
struct vec {
	int x, y, z;
};

typedef struct vec vec_t;


int main()
{
	char *a = "abc", b = 0, **c = NULL;

	b = *(a + 1);
	c = &a;

	//printf("a=%c, b=%c, c=%c\n", *a, b, *(*c+2));
	f(*a, b, *(*c+2), "abc");

	vec_t
		v1 = {1, 2, 3},
		v2 = {3, 2, 1},
		v3 = sum(v1, v2);
	printf("sum: %d, %d, %d\n", v3.x, v3.y, v3.z);
}

void f(int a, b, c, const char *desc)
{
	printf("%s: a=%c, b=%c, c=%c\n", desc, a, b, c);
}

vec_t sum(vec_t a, b)
{
	vec_t s = {};
	s.x = a.x + b.x;
	s.y = a.y + b.y;
	s.z = a.z + b.z;
	return s;
}
