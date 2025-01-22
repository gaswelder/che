#import clip/map
#import test

int main() {
	map.map_t *m = map.new(sizeof(int));

	test.truth("!has a", !map.has(m, "a"));

	int val = 123456;
	map.set(m, "a", &val);
	test.truth("has a", map.has(m, "a"));

	val = 0;
	test.truth("get", map.get(m, "a", &val));
	test.truth("a = 123456", val == 123456);

	return test.fails();
}
