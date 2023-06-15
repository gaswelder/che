#define _XOPEN_SOURCE 700

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#import cli

const char *error = "no error";
pub typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr_in_t;
typedef struct addrinfo addrinfo_t;
typedef struct timeval timeval_t;

#type fd_set
#type socklen_t

#known accept
#known AF_INET
#known AI_PASSIVE
#known ai_protocol
#known ai_socktype
#known bind
#known connect
#known FD_ISSET
#known getaddrinfo
#known IPPROTO_TCP
#known listen
#known select
#known send
#known setsockopt
#known SOCK_STREAM
#known recv
#known MSG_NOSIGNAL
#known ntohs
#known socket
#known close
#known SOL_SOCKET
#known SO_REUSEADDR
#known FD_ZERO
#known FD_SET
#known freeaddrinfo
#known inet_ntoa

pub typedef {
	int fd; // OS's socket (file) descriptor.

	char host[256];
	char port[16];

	int ai_family;
	sockaddr_t ai_addr;
	socklen_t addrlen;
	char addrstr[300];
} net_t;

pub const char *net_error() {
	return error;
}

pub const char *net_addr(net_t *c) {
	return c->addrstr;
}

/**
 * Reads at most size bytes into the buffer buf.
 * Blocks if there is no data ready for reading.
 * Returns the number of bytes read or -1 on error.
 */
pub int readconn(net_t *c, char *buf, size_t size) {
	return recv(c->fd, buf, size, 0);
}

/*
 * Writes 'n' bytes from 'buf' to connection 'c'.
 * Returns 'n' on success or -1 on failure.
 */
pub int net_write(net_t *c, const char *buf, size_t n)
{
	/*
	 * MSG_NOSIGNAL prevents SIGPIPE signals
	 */
	int r = send(c->fd, buf, n, MSG_NOSIGNAL);
	if(r < 0) {
		printf("error %d: %s\n", errno, strerror(errno));
		return r;
	}

	/*
	 * The send call in blocking mode is supposed to make
	 * a full transmission.
	 */
	if((size_t) r < n) {
		cli.fatal("incomplete net_write: %d of %zu (errno=%d, %s)\n",
			r, n, errno, strerror(errno));
	}

	return n;
}

/**
 * Creates a connection with the protocol `proto` to the destination address
 * `addr`. Returns NULL on failure.
 */
pub net_t *net_open(const char *proto, const char *addr)
{
	net_t *c = newconn(proto, addr);
	if(!c) {
		return NULL;
	}

	if (connect(c->fd, &(c->ai_addr), c->addrlen) == -1 ) {
		error = "connect failed";
		free(c);
		return NULL;
	}

	return c;
}

pub net_t *net_listen(const char *proto, const char *addr) {
	net_t *c = newconn(proto, addr);
	if(!c) {
		return NULL;
	}
	int yes = 1;
	if (setsockopt(c->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		error = "setsockopt failed";
		free(c);
		return NULL;
	}
	if (bind(c->fd, &(c->ai_addr), c->addrlen) != 0) {
		error = "bind failed";
		free(c);
		return NULL;
	}
	int backlog = 16;
	if (listen(c->fd, backlog) != 0) {
		error = "listen failed";
		free(c);
		return NULL;
	}
	return c;
}

pub net_t *net_accept(net_t *l) {
	net_t *newconn = calloc(1, sizeof(net_t));
	if (!newconn) {
		error = "no memory for new socket";
		return NULL;
	}
	socklen_t size = sizeof(sockaddr_t);
	int s = accept(l->fd, &(newconn->ai_addr), &size);
	if (s == -1) {
		error = "accept failed";
		free(newconn);
		return NULL;
	}
	if (!format_address(&(newconn->ai_addr), newconn->addrstr, sizeof(newconn->addrstr))) {
		error = "couldn't format address";
		free(newconn);
		close(s);
		return NULL;
	}
	newconn->fd = s;
	return newconn;
}

pub void net_close(net_t *c)
{
	close(c->fd);
	free(c);
}

pub typedef {
	net_t *conn;
	bool read;
	bool write;
} event_t;

pub typedef {
    size_t size;
    event_t events[1000];
	net_t *conns[100];
} pool_t;

pub pool_t *new_pool() {
	pool_t *p = calloc(1, sizeof(pool_t));
	return p;
}

pub bool pool_add(pool_t *p, net_t *conn) {
	for (int i = 0; i < 100; i++) {
		if (p->conns[i]) {
			continue;
		}
		p->conns[i] = conn;
		return true;
	}
	return false;
}

pub bool pool_remove(pool_t *p, net_t *conn) {
	for (int i = 0; i < 100; i++) {
		if (p->conns[i] == conn) {
			p->conns[i] = NULL;
			return true;
		}	
	}
	return false;
}

pub event_t *get_event(pool_t *p) {
    while (p->size == 0) {
		printf("empty, getting events\n");
        fd_set readset = {0};
    	// fd_set writeset = {0};
    	// fd_set errorset = {0};
    	int max = 0;
		for (int i = 0; i < 100; i++) {
			net_t *c = p->conns[i];
			if (!c) {
				continue;
			}
			printf("fd = %d\n", c->fd);
			FD_SET(c->fd, &readset);
			// FD_SET(c->fd, &writeset);
			// FD_SET(c->fd, &errorset);
			if (c->fd > max) {
				max = c->fd;
			}
		}
		// int r = select(max + 1, &readset, &writeset, &errorset, NULL);
		int r = select(max + 1, &readset, NULL, NULL, NULL);
		if (r < 0) {
			return NULL;
		}
		for (int i = 0; i < 100; i++) {
			net_t *c = p->conns[i];
			if (!c) {
				continue;
			}
			p->events[p->size].conn = c;
			p->events[p->size].read = FD_ISSET(c->fd, &readset);
			// p->events[p->size].write = FD_ISSET(c->fd, &writeset);
			printf("fd = %d, r=%d, w=%d\n", c->fd, p->events[p->size].read, p->events[p->size].write);
			p->size++;
		}
    }
	p->size--;
    return &p->events[p->size];
}

/*
 * Creates a connection wrapper for given protocol and address.
 */
net_t *newconn(const char *proto, const char *addr) {
	if (strcmp(proto, "tcp") != 0) {
		error = "Unknown protocol";
		return NULL;
	}
	if (strlen(addr) > 200) {
		error = "address string too long";
		return NULL;
	}
	net_t *c = calloc(1, sizeof(net_t));
	if (!c) {
		error = "malloc failed";
		return NULL;
	}
	strcpy(c->addrstr, addr);
	// Split the address into a hostname and a portname.
	if (!parseaddr(c, addr)) {
		free(c);
		return 0;
	}
	addrinfo_t query = {
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
	};
	addrinfo_t *result = NULL;
	if (getaddrinfo(c->host, c->port, &query, &result) != 0) {
		error = "getaddrinfo error";
		free(c);
		return NULL;
	}
	addrinfo_t *i = NULL;
	for (i = result; i != NULL; i = i->ai_next) {
		c->fd = socket( i->ai_family, i->ai_socktype, i->ai_protocol );
		if (c->fd > 0) {
			memcpy(&(c->ai_addr), i->ai_addr, sizeof(sockaddr_t));
			c->addrlen = i->ai_addrlen;
			c->ai_family = i->ai_family;
			break;
		}
	}
	freeaddrinfo( result );
	if (c->fd <= 0) {
		error = "no suitable addrinfo";
		free(c);
		return NULL;
	}
	return c;
}

int parseaddr(net_t *c, const char *addr)
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

/**
 * Writes the socket a's address into the buffer buf.
 * Returns false on failure.
 */
bool format_address(sockaddr_t *a, char *buf, size_t n) {
	if (a->sa_family != AF_INET) {
		error = "unknown sa_family";
		return false;
	}
	sockaddr_in_t *ai = (sockaddr_in_t *) a;
	int r = snprintf(buf, n, "%s:%u", inet_ntoa(ai->sin_addr), ntohs(ai->sin_port));
	// If r == n, then the string has been trimmed.
	return r > 0 && (size_t) r < n;
}

/*
 * Reads a string from a net connection.
 * Behaves like fgets.
 */
pub char *net_gets(char *s, int size, net_t *c)
{
	int pos = 0;
	// read at most size-1 chars
	for(pos = 0; pos < size-1; pos++) {
		char ch = 0;
		int r = readconn(c, &ch, 1);

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
pub int net_puts(const char *s, net_t *c)
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
pub void net_printf(net_t *c, const char *fmt, ...)
{
	va_list args = {0};
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	char *buf = calloc(len + 1, sizeof(char));
	va_start(args, fmt);
	len = vsnprintf(buf, len+1, fmt, args);
	va_end(args);

	net_puts(buf, c);
	free(buf);
}
