#import json.c
#import test

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
	for (size_t i = 0; i < nelem(cases); i++) {
		case_t c = cases[i];
		json.val_t *v = json.parse(c.in);
		char *s = json.format(v);
		test.streq(s, c.out);
		free(s);
		json.json_free(v);
	}
	return test.fails();
}
