import "clip/str"

int main()
{
	str *s = str_new();

	int c = ' ';
	while(isprint(c)) {
		assert(str_addc(s, c) != false);
		c++;
	}

	assert((size_t)(c - ' ') == str_len(s));
	printf("s='%s'\n", str_raw(s));
	str_free(s);
	puts("OK");
	return 0;
}
