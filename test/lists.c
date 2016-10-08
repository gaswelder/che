
int main()
{
	char *a = "abc", b, **c;

	b = *(a + 1);
	c = &a;

	printf("a=%c, b=%c, c=%c\n", *a, b, *(*c+2));
}
