#import error
#import log
#import os/net

typedef {
	net.conn_t *conn;
	char data[2048];
	size_t len;
} nbuf_t;

typedef {
	int charpos;
	nbuf_t out;
} client_t;

// Character generator server, RFC 864.
// Continuously prints characters to the connected client.
int main() {
	char *address = "0.0.0.0:1900";
	error.t err = {};
	net.conn_t *l = net.listen("tcp", address, &err);
	if (!l) {
		panic("listen failed: %s", err.msg);
	}
	log.logmsg("listening at %s", address);

	while (true) {
		net.conn_t *s = net.accept(l, &err);
		if (!s) {
			fprintf(stderr, "accept failed: %s\n", err.msg);
			continue;
		}
		log.logmsg("%s connected", net.net_addr(s));
		client_t *c = calloc!(1, sizeof(client_t));
		c->out.conn = s;
		process_client(c);
	}
	net.close(l);
	return 0;
}

void *process_client(client_t *c) {
	net.conn_t *conn = c->out.conn;
	char buf[256] = {};
	while (true) {
		if (net.read(conn, buf, sizeof(buf)) == 0) {
			break;
		}
		if (!send_line(c)) {
			fprintf(stderr, "send_line error: %s\n", strerror(errno));
			break;
		}
	}
	log.logmsg("%s disconnected", net.net_addr(conn));
	net.close(conn);
	free(c);
	return NULL;
}

bool send_line(client_t *c) {
	for (int i = 0; i < 72; i++) {
		if (!writech(c, nextch(c))) {
			return false;
		}
	}
	return true;
}

bool writech(client_t *c, char ch) {
	nbuf_t *b = &c->out;
	b->data[b->len++] = ch;
	if (b->len == sizeof(b->data)) {
		int r = net.write(b->conn, b->data, b->len);
		if (r < 0 || (size_t) r < b->len) {
			return false;
		}
		b->len = 0;
	}
	return true;
}

char linechars[] =
	"!\"#$%&'()*+,-./0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~ ";

char nextch(client_t *c) {
	char ch = linechars[c->charpos++];
	if ((size_t) c->charpos == sizeof(linechars)) {
		c->charpos = 0;
	}
	return ch;
}
