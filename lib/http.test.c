#import http
#import test

int main() {
    request();
    url();
	response();
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

void response() {
	const char *data = "HTTP/1.1 200 OK\r\n"
		"Date: Wed, 11 Sep 2024 21:14:21 GMT\r\n"
		"Connection: keep-alive\r\n"
		"Keep-Alive: timeout=5\r\n"
		"Content-Length: 116\r\n"
		"\r\n"
		"d8:completei0e10:incompletei1e8:intervali600e5:peersld2:ip9:127.0.0.17:peer id20:123456789012345678904:porti6881eeee";
	http.response_t r = {};
	test.truth("parse_response", http.parse_response(data, &r));
	test.streq(http.get_res_header(&r, "Keep-Alive"), "timeout=5");
	test.streq(http.get_res_header(&r, "Content-Length"), "116");
	test.truth("content length", r.content_length == 116);
}
