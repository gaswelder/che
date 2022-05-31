#import cli
#import clip/arr

int main()
{
	arr_t *a = arr_new();

	for(int i = 0; i < 1000; i++) {
		if(!arr_pushi(a, i)) {
			fatal("failed");
		}
	}

	size_t n = arr_len(a);
	printf("%zu\n", n);

	arr_t *c = arr_copy(a);
	arr_free(a);

	for(size_t i = 0; i < arr_len(c); i++) {
		printf("%d ", arr_geti(c, i));
	}
	putchar('\n');

	arr_free(c);
}

