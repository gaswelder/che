#define _XOPEN_SOURCE 700

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

import "cli"

const char *error = "no error";
typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr_in_t;
typedef struct addrinfo addrinfo_t;
typedef struct timeval timeval_t;

struct conn {
	int sock;
	char host[256];
	char port[16];

	int ai_family;
	sockaddr_t ai_addr;
	socklen_t addrlen;
	char addrstr[300];
};

typedef struct conn conn_t;

pub const char *net_error() {
	return error;
}

pub const char *net_addr(conn_t *c) {
	return c->addrstr;
}

pub int net_read(conn_t *c, char *buf, size_t size) {
	return recv(c->sock, buf, size, 0);
}

/*
 * Writes 'n' bytes from 'buf' to connection 'c'.
 * Returns 'n' on success or -1 on failure.
 */
pub int net_write(conn_t *c, const char *buf, size_t n)
{
	/*
	 * MSG_NOSIGNAL prevents SIGPIPE signals
	 */
	int r = send(c->sock, buf, n, MSG_NOSIGNAL);
	if(r < 0) {
		printf("error %d: %s\n", errno, strerror(errno));
		return r;
	}

	/*
	 * The send call in blocking mode is supposed to make
	 * a full transmission.
	 */
	if((size_t) r < n) {
		fatal("incomplete net_write: %d of %zu (errno=%d, %s)\n",
			r, n, errno, strerror(errno));
	}

	return n;
}

pub conn_t *net_conn(const char *proto, const char *addr)
{
	conn_t *c = newconn(proto, addr);
	if(!c) {
		return NULL;
	}

	if( connect( c->sock, &(c->ai_addr), c->addrlen ) == -1 ) {
		error = "connect failed";
		free(c);
		return NULL;
	}

	return c;
}

pub conn_t *net_listen(const char *proto, const char *addr)
{
	conn_t *c = newconn(proto, addr);
	if(!c) {
		return NULL;
	}

	int yes = 1;
	if(setsockopt(c->sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		error = "setsockopt failed";
		free(c);
		return NULL;
	}

	if(bind(c->sock, &(c->ai_addr), c->addrlen) != 0) {
		error = "bind failed";
		free(c);
		return NULL;
	}

	if(listen(c->sock, 16) != 0) {
		error = "listen failed";
		free(c);
		return NULL;
	}

	return c;
}

pub conn_t *net_accept(conn_t *l)
{
	conn_t *c = calloc(1, sizeof(conn_t));
	if(!c) {
		error = "no memory for new socket";
		return NULL;
	}

	socklen_t size = sizeof(sockaddr_t);
	int s = accept(l->sock, &(c->ai_addr), &size);
	if(s == -1) {
		error = "accept failed";
		free(c);
		return NULL;
	}

	if(!format_address(&(c->ai_addr), c->addrstr, sizeof(c->addrstr))) {
		error = "couldn't format address";
		free(c);
		close(s);
		return NULL;
	}

	c->sock = s;
	return c;
}

pub void net_close(conn_t *c)
{
	close(c->sock);
	free(c);
}

/*
 * Returns true if there is data to be read
 * from the given connection.
 */
pub int net_incoming(conn_t *c)
{
	fd_set read = {};
	FD_ZERO(&read);
	FD_SET(c->sock, &read);

	timeval_t t = {};
	memset(&t, 0, sizeof(timeval_t));

	if(select(c->sock + 1, &read, NULL, NULL, &t) == -1) {
		error = "select error";
		return 0;
	}

	if(FD_ISSET(c->sock, &read)) {
		return 1;
	}

	return 0;
}

conn_t *newconn(const char *proto, const char *addr)
{
	/*
	 * Only TCP is implemented
	 */
	if(strcmp(proto, "tcp") != 0) {
		error = "Unknown protocol";
		return NULL;
	}

	conn_t *c = calloc(1, sizeof(conn_t));
	if( !c ) {
		error = "malloc failed";
		return NULL;
	}

	if(strlen(addr) > sizeof(c->addrstr)) {
		error = "address string too long";
		return NULL;
	}
	strcpy(c->addrstr, addr);

	if( !getsock(c, addr) ) {
		free(c);
		return NULL;
	}
	return c;
}

int getsock(conn_t *c, const char *addr)
{
	/*
	 * Split the address into a hostname and a portname
	 */
	if(!parseaddr(c, addr)) {
		return 0;
	}

	addrinfo_t query = {
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP
	};
	addrinfo_t *result = NULL;
	if(getaddrinfo(c->host, c->port, &query, &result) != 0) {
		error = "getaddrinfo error";
		return 0;
	}

	addrinfo_t *i = NULL;
	for(i = result; i != NULL; i = i->ai_next)
	{
		c->sock = socket( i->ai_family, i->ai_socktype, i->ai_protocol );
		if( c->sock > 0 ) {
			memcpy(&(c->ai_addr), i->ai_addr, sizeof(sockaddr_t));
			c->addrlen = i->ai_addrlen;
			c->ai_family = i->ai_family;
			break;
		}
	}

	freeaddrinfo( result );
	if( c->sock <= 0 ) {
		error = "no suitable addrinfo";
		return 0;
	}
	return 1;
}

int parseaddr(conn_t *c, const char *addr)
{
	int i = 0;
	const char *p = addr;
	while(*p && *p != ':') {
		if(i >= 255) {
			error = "hostname too long";
			return 0;
		}
		c->host[i++] = *p++;
	}
	c->host[i] = '\0';

	if(*p != ':') {
		error = "missing ':' in the address";
		return 0;
	}

	p++;
	i = 0;
	while(*p) {
		if(i >= 15) {
			error = "portname too long";
			return 0;
		}
		c->port[i++] = *p++;
	}
	c->port[i] = '\0';
	return 1;
}

int format_address(sockaddr_t *a, char *buf, size_t n)
{
	if(a->sa_family != AF_INET) {
		error = "unknown sa_family";
		return 0;
	}
	sockaddr_in_t *ai = (sockaddr_in_t *) a;
	int r = snprintf( buf, n, "%s:%u",
		inet_ntoa( ai->sin_addr ),
		ntohs( ai->sin_port )
	);
	/*
	 * If r == n, then the string has been trimmed.
	 */
	return r > 0 && (size_t) r < n;
}

/*
 * Reads a string from a net connection.
 * Behaves like fgets.
 */
pub char *net_gets(char *s, int size, conn_t *c)
{
	int pos = 0;
	// read at most size-1 chars
	for(pos = 0; pos < size-1; pos++) {
		char ch = 0;
		int r = net_read(c, &ch, 1);

		// on read error return NULL
		if(r < 0) {
			return NULL;
		}

		// if EOF and no chars read, return NULL
		// if there were chars, just stop.
		if(r == 0) {
			if(pos == 0) return NULL;
			else break;
		}

		// Put the char into the output array
		s[pos] = ch;

		// No chars after '\n'
		if(ch == '\n') {
			pos++;
			break;
		}
	}

	// null char after the last char read
	s[pos] = '\0';
	return s;
}

/*
 * Writes a string to a net connection.
 * Behaves like fputs.
 */
pub int net_puts(const char *s, conn_t *c)
{
	size_t len = strlen(s);
	int r = net_write(c, s, len);

	// return EOF if a write error occurs
	if(r < 0 || (size_t) r < len) return EOF;

	// return a non-negative value on success
	return r;
}

/*
 * Writes a formatted string to the connection.
 * Behaves like fprintf.
 */
pub void net_printf(conn_t *c, const char *fmt, ...)
{
	va_list args = {};
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	char buf[len+1] = {};
	va_start(args, fmt);
	len = vsnprintf(buf, len+1, fmt, args);
	va_end(args);

	net_puts(buf, c);
}
