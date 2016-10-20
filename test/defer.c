int main()
{
	test(42);
	return 0;
}

void test(int a)
{
	FILE *f = fopen("_defer.tmp", "w");
	if(!f) return;
	defer fclose(f);

	char *b = malloc(a);
	if(!b) {
		return;
	}
	defer free(b);

	for(int i = 0; i < a; i++) {
		if(b[i] == i) {
			return;
		}
	}

	switch(b[0]) {
		case 42:
			puts("a");
			break;
		case 'b':
		case 'c':
			return;
	}

	puts("b");
}
