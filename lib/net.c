#define _XOPEN_SOURCE 700
/*
 * For some reason the addrinfo struct is not defined
 * unless __USE_POSIX is not explicitly defined here.
 */
#define __USE_POSIX
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


const char *error = "no error";

struct conn {
	int sock;
	char host[256];
	char port[16];

	int ai_family;
	struct sockaddr ai_addr;
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

pub int net_write(conn_t *c, const char *buf, size_t n) {
	return send(c->sock, buf, n, 0);
}

pub conn_t *net_conn(const char *proto, const char *addr)
{
	struct conn *c = newconn(proto, addr);
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
	struct conn *c = newconn(proto, addr);
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
	struct conn *c = calloc(1, sizeof(struct conn));
	if(!c) {
		error = "no memory for new socket";
		return NULL;
	}

	socklen_t size = sizeof(struct sockaddr);
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

pub void net_close(struct conn *c)
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
	fd_set read;
	FD_ZERO(&read);
	FD_SET(c->sock, &read);

	struct timeval t;
	memset(&t, 0, sizeof(struct timeval));

	if(select(c->sock + 1, &read, NULL, NULL, &t) == -1) {
		error = "select error";
		return 0;
	}

	if(FD_ISSET(c->sock, &read)) {
		return 1;
	}

	return 0;
}

struct conn *newconn(const char *proto, const char *addr)
{
	/*
	 * Only TCP is implemented
	 */
	if(strcmp(proto, "tcp") != 0) {
		error = "Unknown protocol";
		return NULL;
	}

	struct conn *c = calloc(1, sizeof(struct conn));
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

int getsock(struct conn *c, const char *addr)
{
	/*
	 * Split the address into a hostname and a portname
	 */
	if(!parseaddr(c, addr)) {
		return 0;
	}

	struct addrinfo query = {
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP
	};
	struct addrinfo *result;
	if(getaddrinfo(c->host, c->port, &query, &result) != 0) {
		error = "getaddrinfo error";
		return 0;
	}

	struct addrinfo *i;
	for(i = result; i != NULL; i = i->ai_next)
	{
		c->sock = socket( i->ai_family, i->ai_socktype, i->ai_protocol );
		if( c->sock > 0 ) {
			memcpy(&(c->ai_addr), i->ai_addr, sizeof(struct sockaddr));
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

int parseaddr(struct conn *c, const char *addr)
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

int format_address(struct sockaddr *a, char *buf, size_t n)
{
	if(a->sa_family != AF_INET) {
		error = "unknown sa_family";
		return 0;
	}
	struct sockaddr_in *ai = (struct sockaddr_in *) a;
	int r = snprintf( buf, n, "%s:%u",
		inet_ntoa( ai->sin_addr ),
		ntohs( ai->sin_port )
	);
	/*
	 * If r == n, then the string has been trimmed.
	 */
	return r > 0 && (size_t) r < n;
}
