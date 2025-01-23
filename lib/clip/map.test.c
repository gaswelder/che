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

	val = 111;
	map.set(m, "a", &val);
	test.truth("get", map.get(m, "a", &val));
	test.truth("a = 111", val == 111);

	char key[8] = {};
	for (int i = 32; i < 80; i++) {
		key[0] = i;
		map.set(m, key, &i);
	}
	for (int i = 32; i < 80; i++) {
		key[0] = i;
		test.truth("get", map.get(m, key, &val));
		test.truth("[key-i] == i", val == i);
	}
	return test.fails();
}
