#import cli
#import clip/arr

int main()
{
	arr_t *a = arr_new();

	int expected = 0;

	for(int i = 0; i < 1000; i++) {
		if(!arr_pushi(a, i)) {
			fatal("failed");
		}
		expected += i;
	}

	arr_t *c = arr_copy(a);

	int actual = 0;
	for (size_t i = 0; i < arr_len(c); i++) {
		actual += arr_geti(c, i);
	}

	if (expected == actual) {
		puts("OK");
		return 0;
	}
	puts("FAIL");
	return 1;
}
