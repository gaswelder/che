import "net"
import "cli"
import "fileutil"
import "strutil"

/*
 * A client request
 */
struct request {
	char method[10];
	char proto[10]; // "HTTP/1.1"
	char path[4080];
};

pub int http_main()
{
	/*
	 * Listen on the given port
	 */
	const char *listen_addr = "0.0.0.0:8080";
	conn_t *ln = net_listen("tcp", listen_addr);
	if(!ln) {
		fatal("Couldn't listen at %s", listen_addr);
	}

	/*
	 * Process clients
	 */
	while(1) {
		conn_t *c = net_accept(ln);
		assert(c);
		process_client(c);
	}

	net_close(ln);
	return 0;
}

/*
 * Processes a single client.
 * The argument is the client's connection.
 */
void *process_client(void *arg)
{
	conn_t *c = (conn_t *) arg;
	defer net_close(c);

	struct request req;

	while(1) {
		if(!read_request(c, &req)) {
			break;
		}
		printf("%s %s\n", req.method, req.path);
		map(c, req.path);
	}

	return NULL;
}

/*
 * Reads a request from the given client
 */
bool read_request(conn_t *c, struct request *req)
{
	char line[4096];

	/*
	 * First line is <method> <path> <protocol>
	 */
	if(!net_gets(line, sizeof(line), c)) {
		puts("gets failed");
		return false;
	}

	char *p = line;
	p = getstr(p, req->method, sizeof(req->method));
	p = getstr(p, req->path, sizeof(req->path));
	p = getstr(p, req->proto, sizeof(req->proto));

	if(strcmp(req->method, "GET") != 0) {
		fatal("Wrong method");
	}

	if(strcmp(req->proto, "HTTP/1.1") != 0) {
		fatal("Wrong protocol");
	}

	/*
	if(!path_valid(path)) {
		fatal("Invalid path");
	}
	*/
	while(1)
	{
		if(!net_gets(line, sizeof(line), c)) {
			fatal("gets failed");
		}
		printf("(%zu) %s\n", strlen(line), line);
		/*
		 * Stop at empty line
		 */
		if(strcmp(line, "\r\n") == 0) {
			break;
		}
	}

	return true;
}

char *getstr(char *src, char *buf, size_t bufsize)
{
	size_t n = 0;
	while(*src) {
		if(*src == ' ') {
			src++;
			break;
		}
		if(*src == '\r') {
			src++;
			assert(*src == '\n');
			src++;
			break;
		}
		if(n >= bufsize) {
			fatal("getstr: buffer too small");
		}
		*buf++ = *src++;
		n++;
	}
	*buf = '\0';
	return src;
}

void error_404(conn_t *c)
{
	net_puts(
		"HTTP/1.1 404 Not Found\r\n"
		"\r\n"
		"Not Found", c);
}
