#import clip/map
#import test

int main() {
	map.map_t *m = map.new(sizeof(int));

	test.truth("!has a", !map.hass(m, "a"));

	int val = 123456;
	map.sets(m, "a", &val);
	test.truth("has a", map.hass(m, "a"));

	val = 0;
	test.truth("get", map.gets(m, "a", &val));
	test.truth("a = 123456", val == 123456);

	val = 111;
	map.sets(m, "a", &val);
	test.truth("get", map.gets(m, "a", &val));
	test.truth("a = 111", val == 111);

	char key[8] = {};
	for (int i = 32; i < 80; i++) {
		key[0] = i;
		map.sets(m, key, &i);
	}
	for (int i = 32; i < 80; i++) {
		key[0] = i;
		test.truth("get", map.gets(m, key, &val));
		test.truth("[key-i] == i", val == i);
	}

	// long key
	val = 34;
	map.sets(m, "1234567890abcdefghijklmnopqrstuvwxyz", &val);
	test.truth("get", map.gets(m, "1234567890abcdefghijklmnopqrstuvwxyz", &val));
	test.truth("val == 34", val == 34);
	return test.fails();
}
