#import json.c
#import mem
#import test
#import writer

int main() {
	testfail("q", "unexpected character: q");
	testfail("[", "unexpected end of input");
	testfail("[1 2", "expected ']', got '2'");
	testok();
	return test.fails();
}

void testfail(const char *in, *out) {
	json.err_t err = {};
	json.val_t *v = json.parse(in, &err);
	test.truth("v == NULL", v == NULL);
	test.truth("err.set", err.set);
	test.streq(err.msg, out);
}

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

void testok() {
	mem.mem_t *m = mem.memopen();
	writer.t *w = mem.newwriter(m);
	json.err_t err = {};

	for (size_t i = 0; i < nelem(cases); i++) {
		case_t c = cases[i];

		// Parse the string
		json.val_t *v = json.parse(c.in, &err);
		if (err.set) {
			panic("parse failed: %s", err.msg);
		}

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
}