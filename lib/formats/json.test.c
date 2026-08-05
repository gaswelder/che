#import json.c
#import test
#import writer
#import error

int main() {
	testfail("q", "unexpected character: q");
	testfail("[", "unexpected end of input");
	testfail("[1 2", "expected ']', got '2'");

	test_reenc("123", "123");
	test_reenc("true", "true");
	test_reenc("null", "null");
	test_reenc("\"abc\"", "\"abc\"");
	test_reenc("[]", "[]");
	test_reenc("{}", "{}");
	test_reenc("123456789", "123456789");
	test_reenc("[1,2,3]", "[1,2,3]");
	test_reenc("[[1,2,3],[4,5,6]]", "[[1,2,3],[4,5,6]]");
	test_reenc(" 123", "123");
	test_reenc("[ 1, 2,3 ]", "[1,2,3]");
	test_reenc("{\"msg\": \"1 2 3\"}", "{\"msg\":\"1 2 3\"}");
	test_reenc("\"foo\\\\nbar\"", "\"foo\\\\nbar\"");

	return test.fails();
}

void testfail(const char *in, *out) {
	error.t err = {};
	json.val_t *v = json.parse(in, &err);
	test.truth("v == NULL", v == NULL);
	test.truth("err.set", err.set);
	test.streq(err.msg, out);
}

void test_reenc(const char *encoded, *expected) {
	error.t err = {};

	// Parse the string.
	json.val_t *v = json.parse(encoded, &err);
	if (err.set) {
		panic("parse failed: %s", err.msg);
	}

	// Format again.
	char buf[100] = {};
	writer.t *w = writer.static_buffer((uint8_t *)buf, 100);
	json.formatwr(w, v);

	// Compare
	test.streq(buf, expected);

	writer.free(w);
	json.json_free(v);
}
