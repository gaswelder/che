import "net"

int main()
{
	conn_t *n = net_conn("tcp", "127.0.0.1:2525");
	if( !n ) {
		printf("%s\n", net_error());
		exit(1);
	}

	sendm(n);

	net_close(n);
}

int sendm(conn_t *n)
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

void expect(conn_t *n, int code)
{
	(void) code;
	if(_error) return;
	
	char buf[256];
	int len = (int) net_read(n, buf, 255);
	if( len < 0 ) {
		printf("net_read error\n");
		_error = 1;
		return;
	}
	buf[len] = 0;
	printf("response: %s\n", buf);
}

void writeline(conn_t *n, const char *str)
{
	if(_error) return;
	net_write(n, str, strlen(str));
	net_write(n, "\r\n", 2);
}

void writestr(conn_t *n, const char *s)
{
	if(_error) return;
	net_write(n, s, strlen(s));
}
