import "os/net"
import "opt"
import "cli"

int main(int argc, char **argv)
{
	const char *addr = "127.0.0.1:25";
	const char *subj = "";

	opt_summary("post [-a addr] [-s subject] <from> <to>");
	opt(OPT_STR, "a", "SMTP server address", &addr);
	opt(OPT_STR, "s", "Mail subject", &subj);
	argv = opt_parse(argc, argv);

	if(!*argv || !*(argv + 1)) {
		err("Missing argument");
		opt_usage();
		return 1;
	}

	const char *from = *argv++;
	const char *to = *argv++;
	if(*argv) {
		err("Unexpected argument: %s\n", *argv);
		opt_usage();
		return 1;
	}

	net_t *n = net_open("tcp", addr);
	if( !n ) {
		fprintf(stderr, "connect to '%s' failed: %s\n", addr, strerror(errno));
		exit(1);
	}

	sendm(n, from, to, subj);
	net_close(n);
}

int sendm(net_t *n, const char *from, const char *to, const char *subj)
{
	expect(n, 220);

	net_printf(n, "HELO %s\r\n", "sofa");
	expect(n, 250);
	if(senderror()) return 0;

	net_printf(n, "MAIL FROM:<%s>\r\n", from);
	expect(n, 250);
	if(senderror()) return 0;

	net_printf(n, "RCPT TO:<%s>\r\n", to);
	expect(n, 250);
	if(senderror()) return 0;

	net_printf(n, "DATA\r\n");
	expect(n, 354);
	if(senderror()) return 0;

	net_printf(n, "Subject: %s\r\n", subj);
	net_printf(n, "From: <%s>\r\n", from);
	net_printf(n, "To: <%s>\r\n", to);
	net_printf(n, "\r\n");

	char buf[4096] = {};
	while( fgets(buf, 4096, stdin) ) {
		net_printf(n, "%s", buf);
	}
	net_printf(n, ".\r\n");
	expect(n, 250);

	net_printf(n, "QUIT\r\n");
	expect(n, 221);

	return !senderror();
}

int _error = 0;

int senderror() {
	return _error;
}

void expect(net_t *n, int code)
{
	(void) code;
	if(_error) return;

	char buf[256] = {};
	int len = net_read(n, buf, 255);
	if( len < 0 ) {
		err("net_read error");
		_error = 1;
		return;
	}
	buf[len] = 0;
	printf("response: %s\n", buf);
}
