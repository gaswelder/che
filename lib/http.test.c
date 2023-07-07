#import http
#import test

int main() {
    request();
    url();
    return test.fails();
}

void request() {
    http.request_t r = {};

    test.truth("parsing", http.parse_request(&r,
        "GET /path/blog/file1.html?a=1&b=2 HTTP/1.0\r\n"
        "Host: example.net\r\n"
        "Accept: application/json; charset=utf-8\r\n\r\n"));

    test.streq(r.method, "GET");
    test.streq(r.uri, "/path/blog/file1.html?a=1&b=2");
    test.streq(r.version, "HTTP/1.0");

    test.streq(r.path, "/path/blog/file1.html");
    test.streq(r.query, "a=1&b=2");
    test.streq(r.filename, "file1.html");

    http.header_t *h = http.get_header(&r, "Accept");
    test.streq(h->name, "Accept");
    test.streq(h->value, "application/json; charset=utf-8");
}

void url() {
    http.url_t r = {};

    test.truth("parsing", http.parse_url(&r, "http://example.net:123/foo/bar?q=1"));
    test.streq(r.schema, "http");
    test.streq(r.hostname, "example.net");
    test.streq(r.port, "123");
    test.streq(r.path, "/foo/bar?q=1");
}
