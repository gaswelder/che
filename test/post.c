import "zio"
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

	zio *n = zopen("tcp", addr, "");
	if( !n ) {
		err("connect failed");
		exit(1);
	}

	sendm(n, from, to, subj);
	zclose(n);
}

int sendm(zio *n, const char *from, const char *to, const char *subj)
{
	expect(n, 220);

	zprintf(n, "HELO %s\r\n", "sofa");
	expect(n, 250);
	if(senderror()) return 0;

	zprintf(n, "MAIL FROM:<%s>\r\n", from);
	expect(n, 250);
	if(senderror()) return 0;

	zprintf(n, "RCPT TO:<%s>\r\n", to);
	expect(n, 250);
	if(senderror()) return 0;

	zprintf(n, "DATA\r\n");
	expect(n, 354);
	if(senderror()) return 0;

	zprintf(n, "Subject: %s\r\n", subj);
	zprintf(n, "From: <%s>\r\n", from);
	zprintf(n, "To: <%s>\r\n", to);
	zprintf(n, "\r\n");

	char buf[4096];
	while( fgets(buf, 4096, stdin) ) {
		zprintf(n, "%s", buf);
	}
	zprintf(n, ".\r\n");
	expect(n, 250);

	zprintf(n, "QUIT\r\n");
	expect(n, 221);

	return !senderror();
}

static int _error = 0;

int senderror() {
	return _error;
}

void expect(zio *n, int code)
{
	(void) code;
	if(_error) return;

	char buf[256];
	int len = zread(n, buf, 255);
	if( len < 0 ) {
		err("net_read error");
		_error = 1;
		return;
	}
	buf[len] = 0;
	printf("response: %s\n", buf);
}
