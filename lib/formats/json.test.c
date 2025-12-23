#import json.c
#import mem
#import test
#import writer

typedef {
	const char *in, *out;
} case_t;

case_t cases[] = {
	{ "123", "123" },
	{ "true", "true" },
	{ "null", "null" },
	{ "\"abc\"", "\"abc\"" },
	{ "[]", "[]" },
	{ "{}", "{}" },
	{ "123456789", "123456789" },
	{ "[1,2,3]", "[1,2,3]" },
	{ "[[1,2,3],[4,5,6]]", "[[1,2,3],[4,5,6]]" },
	{ " 123", "123" },
	{ "[ 1, 2,3 ]", "[1,2,3]" },
	{ "{\"msg\": \"1 2 3\"}", "{\"msg\":\"1 2 3\"}"}
};

int main() {
	mem.mem_t *m = mem.memopen();
	writer.t *w = mem.newwriter(m);

	for (size_t i = 0; i < nelem(cases); i++) {
		case_t c = cases[i];

		// Parse the string
		json.val_t *v = json.parse(c.in);

		// Format again
		char buf[100] = {};
		json.formatwr(w, v);
		mem.rewind(m);
		mem.memread(m, buf, 100);
		mem.reset(m);

		// Compare
		test.streq(buf, c.out);
		json.json_free(v);
	}

	writer.free(w);
	mem.memclose(m);
	return test.fails();
}
