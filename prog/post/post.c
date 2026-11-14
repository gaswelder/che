#import fmt
#import opt
#import os/net
#import writer

int main(int argc, char **argv) {
	char *addr = "127.0.0.1:25";
	char *subj = "no subject";

	opt.nargs(2, "<from> <to>");
	opt.summary("Sends a mail to an SMTP server.");
	opt.str("a", "SMTP server address", &addr);
	opt.str("s", "Mail subject", &subj);
	argv = opt.parse(argc, argv);
	const char *from = *argv++;
	const char *to = *argv++;

	net.net_t *n = net.connect("tcp", addr);
	if (!n) {
		fprintf(stderr, "connect to '%s' failed: %s\n", addr, strerror(errno));
		exit(1);
	}
	writer.t *w = net.getwriter(n);
	sendm(n, w, from, to, subj);
	writer.free(w);
	net.close(n);
	return 0;
}

int sendm(net.net_t *n, writer.t *w, const char *from, const char *to, const char *subj) {
	expect(n, 220);
	if (!senderror()) {
		fmt.wprintf(w, "HELO %s\r\n", "sofa");
		expect(n, 250);
	}
	if (!senderror()) {
		fmt.wprintf(w, "MAIL FROM:<%s>\r\n", from);
		expect(n, 250);
	}
	if (!senderror()) {
		fmt.wprintf(w, "RCPT TO:<%s>\r\n", to);
		expect(n, 250);
	}
	if (!senderror()) {
		fmt.wprintf(w, "DATA\r\n");
		expect(n, 354);
	}
	if (!senderror()) {
		fmt.wprintf(w, "Subject: %s\r\n", subj);
		fmt.wprintf(w, "From: <%s>\r\n", from);
		fmt.wprintf(w, "To: <%s>\r\n", to);
		fmt.wprintf(w, "\r\n");

		char buf[4096] = {};
		while (fgets(buf, 4096, stdin)) {
			fmt.wprintf(w, "%s", buf);
		}
		fmt.wprintf(w, ".\r\n");
		expect(n, 250);

		fmt.wprintf(w, "QUIT\r\n");
		expect(n, 221);
	}
	return !senderror();
}

int _error = 0;

int senderror() {
	return _error;
}

void expect(net.net_t *n, int code) {
	(void) code;
	if (_error) {
		return;
	}

	char buf[256] = {};
	int len = net.read(n, buf, 255);
	if (len < 0) {
		fprintf(stderr, "net.read error: %s\n", strerror(errno));
		_error = 1;
		return;
	}
	buf[len] = 0;
	printf("response: %s\n", buf);
}
