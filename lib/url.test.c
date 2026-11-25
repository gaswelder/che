#import error
#import test
#import url

int main() {
	error.t err = {};
	url.t r = url.parse("http://example.net:123/foo/bar?q=1", &err);
	test.streq(r.schema, "http");
	test.streq(r.hostname, "example.net");
	test.streq(r.port, "123");
	test.streq(r.path, "/foo/bar?q=1");
	return test.fails();
}
