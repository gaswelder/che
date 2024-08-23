#import os/net
#import test

int main() {
	net.ip_addr_t a = {};
	char buf[100] = {};

	test.truth("parse 4", net.parse_addr(&a, "127.0.0.1"));
	test.truth("format 4", net.format_addr(&a, buf, sizeof(buf)));
	test.streq(buf, "127.0.0.1");

	test.truth("parse 6", net.parse_addr(&a, "684D:1111:222:3333:4444:5555:6:77"));
	test.truth("format 6", net.format_addr(&a, buf, sizeof(buf)));
	test.streq(buf, "684d:1111:222:3333:4444:5555:6:77");

	return test.fails();
}
