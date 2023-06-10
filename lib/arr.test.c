#import cli
#import arr

bool test()
{
	arr.arr_t *a = arr.arr_new();

	int expected = 0;

	for(int i = 0; i < 1000; i++) {
		if(!arr.arr_pushi(a, i)) {
			cli.fatal("failed");
		}
		expected += i;
	}

	arr.arr_t *c = arr.arr_copy(a);

	int actual = 0;
	for (size_t i = 0; i < arr.arr_len(c); i++) {
		actual += arr.arr_geti(c, i);
	}

	return expected == actual;
}

int main() {
	if (test()) {
		return 0;
	}
	return 1;
}
