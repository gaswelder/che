static const char *error = "no error";

struct conn {
	int sock;
	char host[256];
	char port[16];
	struct sockaddr ai_addr;
	socklen_t addrlen;
};

typedef struct conn conn_t;

const char *net_error() {
	return error;
}

int net_read(conn_t *c, char *buf, size_t size) {
	return recv(c->sock, buf, size, 0);
}

int net_write(conn_t *c, const char *buf, size_t n) {
	return send(c->sock, buf, n, 0);
}

conn_t *net_conn(const char *proto, const char *addr)
{
	/*
	 * Only TCP is implemented
	 */
	if( strcmp(proto, "tcp") != 0 ) {
		error = "Unknown protocol";
		return NULL;
	}

	struct conn *c = calloc(1, sizeof(struct conn));
	if( !c ) {
		error = "malloc failed";
		return NULL;
	}

	/*
	 * Split the address into a hostname and a portname
	 */
	if( !parseaddr( c, addr ) ) {
		free( c );
		return NULL;
	}

	if( !getsock(c) ) {
		free(c);
		return NULL;
	}

	/*
	 * Connect
	 */
	if( connect( c->sock, &(c->ai_addr), c->addrlen ) == -1 ) {
		error = "connect failed";
		free(c);
		return NULL;
	}
	puts("3");

	return c;
}

void net_close(struct conn *c)
{
	close(c->sock);
	free(c);
}

static int parseaddr(struct conn *c, const char *addr)
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

static int getsock(struct conn *c)
{
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
