#import test
#import enc/urlencode
#import writer

int main() {
	uint8_t buf[100] = {};
	writer.t *w = writer.static_buffer(buf, sizeof(buf));

	const char *msg = "msg/hello world!";
	const char *out = "msg%2Fhello%20world%21";
	int len = urlencode.write(w, msg, strlen(msg));
	test.truth("len check", (size_t) len == strlen(out));
	test.streq((char *)buf, out);
	return test.fails();
}
