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

typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_storage sockaddr_storage_t; // IPv6
typedef struct sockaddr_in sockaddr_in_t;
typedef struct addrinfo addrinfo_t;

const size_t MAX_ADDR = 300;

typedef {
	char host[256];
	char port[16];
	sockaddr_storage_t ai_addr;
	socklen_t addrlen;
} internal_t;

pub typedef {
	int fd; // OS's socket (file) descriptor.
	char addrstr[MAX_ADDR];
	bool is_listener;
	void *internal; // Private internal_t owned by this connection.
} conn_t;

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

pub const char *net_addr(conn_t *c) {
	return c->addrstr;
}

// Reads at most size bytes into the buffer buf.
// Blocks if there is no data ready for reading.
// Returns the number of bytes read or -1 on error.
pub int read(conn_t *c, char *buf, size_t size) {
	return OS.recv(c->fd, buf, size, 0);
}

// Writes n bytes from buf to connection c.
// Returns the number of bytes sent, or -1 on failure.
pub int write(conn_t *c, const char *buf, size_t n) {
	// MSG_NOSIGNAL prevents SIGPIPE signals
	return OS.send(c->fd, buf, n, OS.MSG_NOSIGNAL);
}

// Connects to addr over protocol proto and returns the connection.
// On failure returns NULL and sets err.
pub conn_t *connect(const char *proto, const char *addr, error.t *err) {
	conn_t *c = newconn(proto, addr, err);
	if (!c) {
		return NULL;
	}
	internal_t *state = c->internal;
	if (OS.connect(c->fd, (sockaddr_t *)&state->ai_addr, state->addrlen) < 0) {
		error.set(err, "%s", strerror(errno));
		close(c);
		return NULL;
	}
	return c;
}

// Same as connect, but doesn't block and returns immediately without
// waiting for the actual connection to happen.
pub conn_t *connect_nonblock(const char *proto, *addr, error.t *err) {
	conn_t *c = newconn(proto, addr, err);
	if (!c) {
		return NULL;
	}
	internal_t *state = c->internal;
	int flags = OS.fcntl(c->fd, OS.F_GETFL, 0);
	if (flags < 0) {
		error.set(err, "fcntl failed: %s", strerror(errno));
		close(c);
		return NULL;
	}
	if (OS.fcntl(c->fd, OS.F_SETFL, flags | OS.O_NONBLOCK) < 0) {
		error.set(err, "fcntl failed: %s", strerror(errno));
		close(c);
		return NULL;
	}
	if (OS.connect(c->fd, (sockaddr_t *)&state->ai_addr, state->addrlen) < 0) {
		if (errno == OS.EINPROGRESS) {
			return c;
		}
		error.set(err, "%s", strerror(errno));
		close(c);
		return NULL;
	}
	return c;
}

// Creates a listener connection at the given local address.
pub conn_t *listen(const char *proto, const char *addr, error.t *err) {
	conn_t *c = newconn(proto, addr, err);
	if (!c) {
		return NULL;
	}
	internal_t *state = c->internal;
	c->is_listener = true;

	int yes = 1;
	if (OS.setsockopt(c->fd, OS.SOL_SOCKET, OS.SO_REUSEADDR, &yes, sizeof(int)) != 0) {
		error.set(err, "setsockopt failed: %s", strerror(errno));
		close(c);
		return NULL;
	}
	if (OS.bind(c->fd, (sockaddr_t *)&state->ai_addr, state->addrlen) != 0) {
		error.set(err, "bind failed: %s", strerror(errno));
		close(c);
		return NULL;
	}
	int backlog = 16;
	if (OS.listen(c->fd, backlog) != 0) {
		error.set(err, "listen failed: %s", strerror(errno));
		close(c);
		return NULL;
	}
	return c;
}

// Waits for a new connection on the listener l.
// Returns the connection or an error.
pub conn_t *accept(conn_t *l, error.t *err) {
	conn_t *conn = allocconn();
	internal_t *state = conn->internal;
	socklen_t size = sizeof(sockaddr_storage_t);
	int fd = OS.accept(l->fd, (sockaddr_t *)&state->ai_addr, &size);
	if (fd == -1) {
		error.set(err, "accept failed: %s", strerror(errno));
		close(conn);
		return NULL;
	}
	conn->fd = fd;
	state->addrlen = size;
	if (!format_address((sockaddr_t *)&state->ai_addr, conn->addrstr, sizeof(conn->addrstr), err)) {
		close(conn);
		return NULL;
	}
	return conn;
}

// Closes connection c and frees its private state.
pub void close(conn_t *c) {
	if (c->fd >= 0) OS.close(c->fd);
	free(c->internal);
	free(c);
}

conn_t *allocconn() {
	conn_t *c = calloc!(1, sizeof(conn_t));
	c->fd = -1;
	c->internal = calloc!(1, sizeof(internal_t));
	return c;
}

// Creates a connection wrapper for the given protocol and address.
conn_t *newconn(const char *proto, const char *addr, error.t *err) {
	if (strcmp(proto, "tcp") != 0) {
		error.set(err, "Unknown protocol");
		return NULL;
	}
	if (strlen(addr) >= MAX_ADDR) {
		error.set(err, "address string too long");
		return NULL;
	}
	conn_t *c = allocconn();
	internal_t *state = c->internal;

	strcpy(c->addrstr, addr);
	// Split the address into a hostname and a portname.
	if (!parseaddr(state, addr, err)) {
		close(c);
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
	if (OS.getaddrinfo(state->host, state->port, &hints, &result) != 0) {
		error.set(err, "getaddrinfo error");
		close(c);
		return NULL;
	}
	for (addrinfo_t *i = result; i != NULL; i = i->ai_next) {
		if (i->ai_addrlen > sizeof(state->ai_addr)) continue;
		c->fd = OS.socket(i->ai_family, i->ai_socktype, i->ai_protocol);
		if (c->fd > 0) {
			memcpy(&state->ai_addr, i->ai_addr, i->ai_addrlen);
			state->addrlen = i->ai_addrlen;
			break;
		}
	}
	OS.freeaddrinfo(result);
	if (c->fd < 0) {
		error.set(err, "no suitable addrinfo");
		close(c);
		return NULL;
	}
	return c;
}

int parseaddr(internal_t *state, const char *addr, error.t *err) {
	int i = 0;
	const char *p = addr;
	while (*p != '\0' && *p != ':') {
		if (i >= 255) {
			error.set(err, "hostname too long");
			return 0;
		}
		state->host[i++] = *p++;
	}
	state->host[i] = '\0';

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
		state->port[i++] = *p++;
	}
	state->port[i] = '\0';
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
pub reader.t *getreader(conn_t *conn) {
	reader.t *r = calloc!(1, sizeof(reader.t));
	r->data = conn;
	r->read = reader_read;
	return r;
}

int reader_read(void *ctx, uint8_t *buf, size_t n) {
	conn_t *conn = ctx;
	return OS.read(conn->fd, buf, n);
}

pub writer.t *getwriter(conn_t *conn) {
	return writer.new(conn, writer_write, NULL);
}

int writer_write(void *ctx, const uint8_t *buf, size_t n) {
	conn_t *conn = ctx;
	return OS.write(conn->fd, buf, n);
}
