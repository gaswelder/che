#import arr
#import test

int main() {
	arr.arr_t *a = arr.arr_new();
	int expected = 0;
	for (int i = 0; i < 1000; i++) {
		if (!test.truth("arr_pushi", arr.arr_pushi(a, i))) {
			break;
		}
		expected += i;
	}
	arr.arr_t *c = arr.arr_copy(a);
	int actual = 0;
	for (size_t i = 0; i < arr.arr_len(c); i++) {
		actual += arr.arr_geti(c, i);
	}
	test.truth("sums comparison", expected == actual);
	return test.fails();
}
