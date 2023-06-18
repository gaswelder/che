#import http
#import test

int main() {
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
    return test.fails();
}
