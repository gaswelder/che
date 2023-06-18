#import http
#import test

int main() {
    http.request_line_t r = {};
    test.truth("parsing", http.parse_request_line("GET /path/blog/file1.html?a=1&b=2 HTTP/1.0", &r));
    test.streq(r.method, "GET");
    test.streq(r.uri, "/path/blog/file1.html?a=1&b=2");
    test.streq(r.version, "HTTP/1.0");

    test.streq(r.path, "/path/blog/file1.html");
    test.streq(r.query, "a=1&b=2");
    test.streq(r.filename, "file1.html");

    http.header_t h = {};
    test.truth("header parsing", http.parse_header_line("Content-Type: application/json; charset=utf-8", &h));
    test.streq(h.name, "Content-Type");
    test.streq(h.value, "application/json; charset=utf-8");
    return test.fails();
}
