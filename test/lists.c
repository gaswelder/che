
struct vec {
	int x, y, z;
};


int main()
{
	char *a = "abc", b, **c;

	b = *(a + 1);
	c = &a;

	//printf("a=%c, b=%c, c=%c\n", *a, b, *(*c+2));
	f(*a, b, *(*c+2), "abc");

	struct vec
		v1 = {1, 2, 3},
		v2 = {3, 2, 1},
		v3 = sum(v1, v2);
	printf("sum: %d, %d, %d\n", v3.x, v3.y, v3.z);
}

void f(int a, b, c, const char *desc)
{
	printf("%s: a=%c, b=%c, c=%c\n", desc, a, b, c);
}

struct vec sum(struct vec a, b)
{
	struct vec s;
	s.x = a.x + b.x;
	s.y = a.y + b.y;
	s.z = a.z + b.z;
	return s;
}
