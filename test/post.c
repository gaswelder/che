import "zio"

int main()
{
	zio *n = zopen("tcp", "127.0.0.1:2525", "");
	if( !n ) {
		puts("connect failed");
		exit(1);
	}

	sendm(n);
	zclose(n);
}

int sendm(zio *n)
{
	expect(n, 220);
	writeline(n, "HELO sofa");

	expect(n, 250);

	writeline(n, "MAIL FROM:<nobody@localhost>");
	expect(n, 250);

	writeline(n, "RCPT TO:<bob@localhost>");
	expect(n, 250);

	writeline(n, "DATA");
	expect(n, 354);

	writeline(n, "Subject: test");
	writeline(n, "From: <nobody@localhost>");
	writeline(n, "To: <bob@localhost>");
	writeline(n, "");
	char buf[4096];
	while( fgets(buf, 4096, stdin) ) {
		writestr(n, buf);
	}
	writeline(n, ".");
	expect(n, 250);


	writeline(n, "QUIT");
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
		printf("net_read error\n");
		_error = 1;
		return;
	}
	buf[len] = 0;
	printf("response: %s\n", buf);
}

void writeline(zio *n, const char *str)
{
	if(_error) return;
	zwrite(n, str, strlen(str));
	zwrite(n, "\r\n", 2);
}

void writestr(zio *n, const char *s)
{
	if(_error) return;
	zwrite(n, s, strlen(s));
}
