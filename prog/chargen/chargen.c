/*
 * Character generator server, RFC 864.
 *
 * All this does is continuously printing character data to any
 * connected client.
 */

// chargen -l <listen addr>

#import os/net
#import log

typedef {
	net.net_t *conn;
	char data[2048];
	int len;
} nbuf_t;

typedef {
	int charpos;
	nbuf_t out;
} client_t;

char linechars[] =
	"!\"#$%&'()*+,-./0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~ ";

int main()
{
	char address[] = "0.0.0.0:1900";
	net.net_t *l = net.net_listen("tcp", address);
	if(!l) {
		panic("listen failed: %s", net.net_error());
	}
	log.logmsg("listening at %s", address);

	while(1) {
		net.net_t *s = net.net_accept(l);
		if(!s) {
			fprintf(stderr, "accept failed: %s\n", strerror(errno));
			continue;
		}

		client_t *c = calloc(1, sizeof(client_t));
		c->out.conn = s;
		process_client(c);
		//th_detach(th_start(&process_client, c));
	}

	net.net_close(l);
	return 0;
}

void *process_client(client_t *c)
{
	net.net_t *conn = c->out.conn;
	log.logmsg("%s connected", net.net_addr(conn));
	char buf[256] = {};

	while(1) {
		if (net.readconn(conn, buf, sizeof(buf)) == 0) {
			break;
		}

		if(!send_line(c)) {
			fprintf(stderr, "send_line error: %s\n", strerror(errno));
			break;
		}
	}
	log.logmsg("%s disconnected", net.net_addr(conn));
	net.net_close(conn);
	free(c);
	return NULL;
}

bool send_line(client_t *c)
{
	for(int i = 0; i < 72; i++) {
		/*
		 * Get next character
		 */
		char ch = linechars[c->charpos];
		c->charpos++;
		if(c->charpos == sizeof(linechars)) {
			c->charpos = 0;
		}

		/*
		 * Put it to the client's output buffer
		 */
		if(putch(ch, &(c->out)) == EOF) {
			return false;
		}
	}
	return true;
}

/*
 * Puts a character to the client output buffer.
 * Flushes the buffer when it fills.
 */
int putch(int ch, nbuf_t *b)
{
	b->data[b->len++] = ch;
	if(b->len == sizeof(b->data)) {
		if(net.net_write(b->conn, b->data, b->len) < b->len) {
			return EOF;
		}
		b->len = 0;
	}
	return ch;
}
