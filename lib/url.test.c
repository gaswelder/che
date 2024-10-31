#import url
#import test

int main() {
    url.t *r = url.parse("http://example.net:123/foo/bar?q=1");
    test.streq(r->schema, "http");
    test.streq(r->hostname, "example.net");
    test.streq(r->port, "123");
    test.streq(r->path, "/foo/bar?q=1");
	free(r);
    return test.fails();
}
