#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#type socklen_t

#import error
#import reader
#import writer

pub typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr_in_t;
typedef struct addrinfo addrinfo_t;

const size_t MAX_ADDR = 300;

pub typedef {
	int fd; // OS's socket (file) descriptor.

	char host[256];
	char port[16];

	int ai_family;
	sockaddr_t ai_addr;
	socklen_t addrlen;
	char addrstr[MAX_ADDR];
	bool is_listener;
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

pub const char *net_addr(net_t *c) {
	return c->addrstr;
}

// Reads at most size bytes into the buffer buf.
// Blocks if there is no data ready for reading.
// Returns the number of bytes read or -1 on error.
pub int read(net_t *c, char *buf, size_t size) {
	return OS.recv(c->fd, buf, size, 0);
}

// Writes n bytes from buf to connection c.
// Returns the number of bytes sent, or -1 on failure.
pub int write(net_t *c, const char *buf, size_t n) {
	// MSG_NOSIGNAL prevents SIGPIPE signals
	return OS.send(c->fd, buf, n, OS.MSG_NOSIGNAL);
}

// Connects to addr over protocol proto and returns the connection.
// On failure returns NULL and sets err.
pub net_t *connect(const char *proto, const char *addr, error.t *err) {
	net_t *c = newconn(proto, addr, err);
	if (!c) {
		return NULL;
	}
	if (OS.connect(c->fd, &c->ai_addr, c->addrlen) < 0) {
		error.set(err, "%s", strerror(errno));
		OS.close(c->fd);
		free(c);
		return NULL;
	}
	return c;
}

// Same as connect, but doesn't block and returns immediately without
// waiting for the actual connection to happen.
pub net_t *connect_nonblock(const char *proto, *addr, error.t *err) {
	net_t *c = newconn(proto, addr, err);
	if (!c) {
		return NULL;
	}
	int flags = OS.fcntl(c->fd, OS.F_GETFL, 0);
	if (flags < 0) {
		error.set(err, "fcntl failed: %s", strerror(errno));
		OS.close(c->fd);
		free(c);
		return NULL;
	}
	if (OS.fcntl(c->fd, OS.F_SETFL, flags | OS.O_NONBLOCK) < 0) {
		error.set(err, "fcntl failed: %s", strerror(errno));
		OS.close(c->fd);
		free(c);
		return NULL;
	}
	if (OS.connect(c->fd, (sockaddr_t *)&c->ai_addr, c->addrlen) < 0) {
		if (errno == OS.EINPROGRESS) {
			return c;
		}
		error.set(err, "%s", strerror(errno));
		OS.close(c->fd);
		free(c);
		return NULL;
	}
	return c;
}

// getsockopt(...SO_ERROR...)
// int get_socket_error(int fd) {
// 	int error = 0;
// 	size_t n = sizeof(error);
// // so_error may contain connection error on a non-blocking socket.
// 	if (getsockopt(fd, SOL_SOCKET,SO_ERROR, &error, &n) < 0) {
// 		return -1;
// 	}
// 	return error;
// }
pub net_t *net_listen(const char *proto, const char *addr, error.t *err) {
	net_t *c = newconn(proto, addr, err);
	if (!c) {
		return NULL;
	}
	c->is_listener = true;

	int yes = 1;
	if (OS.setsockopt(c->fd, OS.SOL_SOCKET, OS.SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		error.set(err, "setsockopt failed: %s", strerror(errno));
		OS.close(c->fd);
		free(c);
		return NULL;
	}
	if (OS.bind(c->fd, (sockaddr_t *)&c->ai_addr, c->addrlen) != 0) {
		error.set(err, "bind failed: %s", strerror(errno));
		OS.close(c->fd);
		free(c);
		return NULL;
	}
	int backlog = 16;
	if (OS.listen(c->fd, backlog) != 0) {
		error.set(err, "listen failed: %s", strerror(errno));
		OS.close(c->fd);
		free(c);
		return NULL;
	}
	return c;
}

pub net_t *net_accept(net_t *l, error.t *err) {
	net_t *newconn = calloc!(1, sizeof(net_t));
	socklen_t size = sizeof(sockaddr_t);
	int s = OS.accept(l->fd, &newconn->ai_addr, &size);
	if (s == -1) {
		error.set(err, "accept failed: %s", strerror(errno));
		free(newconn);
		return NULL;
	}
	if (!format_address((sockaddr_t *)&newconn->ai_addr, newconn->addrstr, sizeof(newconn->addrstr), err)) {
		free(newconn);
		OS.close(s);
		return NULL;
	}
	newconn->fd = s;
	return newconn;
}

// Closes connection c.
pub void close(net_t *c) {
	OS.close(c->fd);
	free(c);
}

// Creates a connection wrapper for the given protocol and address.
net_t *newconn(const char *proto, const char *addr, error.t *err) {
	if (strcmp(proto, "tcp") != 0) {
		error.set(err, "Unknown protocol");
		return NULL;
	}
	if (strlen(addr) >= MAX_ADDR) {
		error.set(err, "address string too long");
		return NULL;
	}
	net_t *c = calloc!(1, sizeof(net_t));
	c->fd = -1;

	strcpy(c->addrstr, addr);
	// Split the address into a hostname and a portname.
	if (!parseaddr(c, addr, err)) {
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
		error.set(err, "getaddrinfo error");
		free(c);
		return NULL;
	}
	for (addrinfo_t *i = result; i != NULL; i = i->ai_next) {
		if (i->ai_addrlen > sizeof(c->ai_addr)) continue;
		c->fd = OS.socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if (c->fd > 0) {
			memcpy(&c->ai_addr, i->ai_addr, sizeof(sockaddr_t));
			c->addrlen = i->ai_addrlen;
			c->ai_family = i->ai_family;
			break;
		}
	}
	OS.freeaddrinfo(result);
	if (c->fd < 0) {
		error.set(err, "no suitable addrinfo");
		free(c);
		return NULL;
	}
	return c;
}

int parseaddr(net_t *c, const char *addr, error.t *err) {
	int i = 0;
	const char *p = addr;
	while (*p != '\0' && *p != ':') {
		if (i >= 255) {
			error.set(err, "hostname too long");
			return 0;
		}
		c->host[i++] = *p++;
	}
	c->host[i] = '\0';

	if (*p != ':') {
		error.set(err, "missing ':' in the address");
		return 0;
	}

	p++;
	i = 0;
	while (*p != '\0') {
		if (i >= 15) {
			error.set(err, "portname too long");
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
bool format_address(sockaddr_t *a, char *buf, size_t n, error.t *err) {
	if (a->sa_family != OS.AF_INET) {
		error.set(err, "unknown sa_family");
		return false;
	}
	sockaddr_in_t *ai = (sockaddr_in_t *) a;
	int r = snprintf(buf, n, "%s:%u", OS.inet_ntoa(ai->sin_addr), OS.ntohs(ai->sin_port));
	if (r <= 0 || (size_t) r >= n) {
		error.set(err, "couldn't format address");
		return false;
	}
	return true;
}

// Returns a reader interface for the given connection.
// Freeing the reader will not close the connection, the caller will still own it.
pub reader.t *getreader(net_t *conn) {
	reader.t *r = calloc!(1, sizeof(reader.t));
	r->data = conn;
	r->read = reader_read;
	return r;
}

int reader_read(void *ctx, uint8_t *buf, size_t n) {
	net_t *conn = ctx;
	return OS.read(conn->fd, buf, n);
}

pub writer.t *getwriter(net_t *conn) {
	return writer.new(conn, writer_write, NULL);
}

int writer_write(void *ctx, const uint8_t *buf, size_t n) {
	net_t *conn = ctx;
	return OS.write(conn->fd, buf, n);
}
