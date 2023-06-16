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
    return test.fails();
}
