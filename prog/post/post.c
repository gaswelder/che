#import os/net
#import opt

int main(int argc, char **argv) {
	char *addr = "127.0.0.1:25";
	char *subj = "";

	opt.opt_summary("post [-a addr] [-s subject] <from> <to>");
	opt.str("a", "SMTP server address", &addr);
	opt.str("s", "Mail subject", &subj);
	argv = opt.opt_parse(argc, argv);
	if (!*argv || !*(argv + 1)) return opt.usage();

	const char *from = *argv++;
	const char *to = *argv++;
	if (*argv) {
		fprintf(stderr, "Unexpected argument: %s\n", *argv);
		return opt.usage();
	}

	net.net_t *n = net.net_open("tcp", addr);
	if (!n) {
		fprintf(stderr, "connect to '%s' failed: %s\n", addr, strerror(errno));
		exit(1);
	}
	sendm(n, from, to, subj);
	net.net_close(n);
	return 0;
}

int sendm(net.net_t *n, const char *from, const char *to, const char *subj)
{
	expect(n, 220);

	net.net_printf(n, "HELO %s\r\n", "sofa");
	expect(n, 250);
	if(senderror()) return 0;

	net.net_printf(n, "MAIL FROM:<%s>\r\n", from);
	expect(n, 250);
	if(senderror()) return 0;

	net.net_printf(n, "RCPT TO:<%s>\r\n", to);
	expect(n, 250);
	if(senderror()) return 0;

	net.net_printf(n, "DATA\r\n");
	expect(n, 354);
	if(senderror()) return 0;

	net.net_printf(n, "Subject: %s\r\n", subj);
	net.net_printf(n, "From: <%s>\r\n", from);
	net.net_printf(n, "To: <%s>\r\n", to);
	net.net_printf(n, "\r\n");

	char buf[4096] = {};
	while( fgets(buf, 4096, stdin) ) {
		net.net_printf(n, "%s", buf);
	}
	net.net_printf(n, ".\r\n");
	expect(n, 250);

	net.net_printf(n, "QUIT\r\n");
	expect(n, 221);

	return !senderror();
}

int _error = 0;

int senderror() {
	return _error;
}

void expect(net.net_t *n, int code)
{
	(void) code;
	if(_error) return;

	char buf[256] = {};
	int len = net.readconn(n, buf, 255);
	if (len < 0) {
		fprintf(stderr, "net.readconn error: %s\n", strerror(errno));
		_error = 1;
		return;
	}
	buf[len] = 0;
	printf("response: %s\n", buf);
}
