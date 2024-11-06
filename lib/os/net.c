// #define _XOPEN_SOURCE 700

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

const char *error = "no error";
pub typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr_in_t;
typedef struct addrinfo addrinfo_t;

#type fd_set
#type socklen_t

pub typedef {
	int fd; // OS's socket (file) descriptor.

	char host[256];
	char port[16];

	int ai_family;
	sockaddr_t ai_addr;
	socklen_t addrlen;
	char addrstr[300];
} net_t;

pub typedef {
	char v; // 4 or 6
	// 4 bytes for ip4 (struct in_addr),
	// 16 bytes for ip6 (struct in6_addr)
	char data[16];
} ip_addr_t;

/**
 * Parses a string address into the given address a.
 * Returns false on failure.
 */
pub bool parse_addr(ip_addr_t *a, const char *str) {
	if (OS.inet_pton(OS.AF_INET6, str, a->data)) {
		a->v = 6;
		return true;
	}
	if (OS.inet_pton(OS.AF_INET, str, a->data)) {
		a->v = 4;
		return true;
	}
	return false;
}

/**
 * Formats an address writing to buf.
 * Returns false on failure.
 */
pub bool format_addr(ip_addr_t *a, char *buf, size_t bufsize) {
	int af = 0;
	switch (a->v) {
		case 4: { af = OS.AF_INET; }
		case 6: { af = OS.AF_INET6; }
	}
	return OS.inet_ntop(af, a->data, buf, bufsize) != NULL;
}

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
	return OS.recv(c->fd, buf, size, 0);
}

/*
 * Writes 'n' bytes from 'buf' to connection 'c'.
 * Returns 'n' on success or -1 on failure.
 */
pub int net_write(net_t *c, const char *buf, size_t n) {
	/*
	 * MSG_NOSIGNAL prevents SIGPIPE signals
	 */
	int r = OS.send(c->fd, buf, n, OS.MSG_NOSIGNAL);
	if (r < 0) {
		printf("error %d: %s\n", errno, strerror(errno));
		return r;
	}
	/*
	 * The send call in blocking mode is supposed to make
	 * a full transmission.
	 */
	if ((size_t) r < n) {
		panic("incomplete net_write: %d of %zu (errno=%d, %s)\n",
			r, n, errno, strerror(errno));
	}
	return n;
}

// Creates a connection over the protocol proto ("tcp") to the destination
// address addr.
// On failure return NULL, check errno.
pub net_t *connect(const char *proto, const char *addr) {
	net_t *c = newconn(proto, addr);
	if (!c) return NULL;
	if (OS.connect(c->fd, &(c->ai_addr), c->addrlen) < 0) {
		free(c);
		return NULL;
	}
	return c;
}

// Same as connect, but doesn't block and returns immediately without
// waiting for the actual connection to happen.
pub net_t *connect_nonblock(const char *proto, *addr) {
	net_t *c = newconn(proto, addr);
	if (!c) return NULL;
	int flags = OS.fcntl(c->fd, OS.F_GETFL, 0);
	if (flags < 0) panic("fcntl failed");
	if (OS.fcntl(c->fd, OS.F_SETFL, flags | OS.O_NONBLOCK) < 0) panic("fcntl failed");
	if (OS.connect(c->fd, &(c->ai_addr), c->addrlen) < 0) {
		if (errno == OS.EINPROGRESS) return c;
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
	if (OS.setsockopt(c->fd, OS.SOL_SOCKET, OS.SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		error = "setsockopt failed";
		free(c);
		return NULL;
	}
	if (OS.bind(c->fd, &(c->ai_addr), c->addrlen) != 0) {
		error = "bind failed";
		free(c);
		return NULL;
	}
	int backlog = 16;
	if (OS.listen(c->fd, backlog) != 0) {
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
	int s = OS.accept(l->fd, &(newconn->ai_addr), &size);
	if (s == -1) {
		error = "accept failed";
		free(newconn);
		return NULL;
	}
	if (!format_address(&(newconn->ai_addr), newconn->addrstr, sizeof(newconn->addrstr))) {
		error = "couldn't format address";
		free(newconn);
		OS.close(s);
		return NULL;
	}
	newconn->fd = s;
	return newconn;
}

pub void net_close(net_t *c) {
	OS.close(c->fd);
	free(c);
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

	// filter, specifies what addresses we are interested in.
	addrinfo_t hints = {
		// int ai_flags
		// int ai_family // AF_INET, AF_INET6
		// socklen_t ai_addrlen
		// struct sockaddr *ai_addr
		// char *ai_canonname
		// struct addrinfo *ai_next
		.ai_socktype = OS.SOCK_STREAM, // or SOCK_DGRAM
		.ai_protocol = OS.IPPROTO_TCP,
	};
	addrinfo_t *result = NULL;
	if (OS.getaddrinfo(c->host, c->port, &hints, &result) != 0) {
		error = "getaddrinfo error";
		free(c);
		return NULL;
	}
	addrinfo_t *i = NULL;
	for (i = result; i != NULL; i = i->ai_next) {
		c->fd = OS.socket( i->ai_family, i->ai_socktype, i->ai_protocol );
		if (c->fd > 0) {
			memcpy(&(c->ai_addr), i->ai_addr, sizeof(sockaddr_t));
			c->addrlen = i->ai_addrlen;
			c->ai_family = i->ai_family;
			break;
		}
	}
	OS.freeaddrinfo( result );
	if (c->fd <= 0) {
		error = "no suitable addrinfo";
		free(c);
		return NULL;
	}
	return c;
}

int parseaddr(net_t *c, const char *addr) {
	int i = 0;
	const char *p = addr;
	while (*p && *p != ':') {
		if (i >= 255) {
			error = "hostname too long";
			return 0;
		}
		c->host[i++] = *p++;
	}
	c->host[i] = '\0';

	if (*p != ':') {
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
	if (a->sa_family != OS.AF_INET) {
		error = "unknown sa_family";
		return false;
	}
	sockaddr_in_t *ai = (sockaddr_in_t *) a;
	int r = snprintf(buf, n, "%s:%u", OS.inet_ntoa(ai->sin_addr), OS.ntohs(ai->sin_port));
	// If r == n, then the string has been trimmed.
	return r > 0 && (size_t) r < n;
}

/*
 * Writes a string to a net connection.
 * Behaves like fputs.
 */
int net_puts(const char *s, net_t *c) {
	size_t len = strlen(s);
	int r = net_write(c, s, len);

	// return EOF if a write error occurs
	if (r < 0 || (size_t) r < len) {
		return EOF;
	}

	// return a non-negative value on success
	return r;
}

/*
 * Writes a formatted string to the connection.
 * Behaves like fprintf.
 */
pub void net_printf(net_t *c, const char *fmt, ...) {
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
