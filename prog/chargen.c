/*
 * Character generator server, RFC 864.
 *
 * All this does is continuously printing character data to any
 * connected client.
 */

// chargen -l <listen addr>

import "net"
import "cli"
import "zio"
import "log"

struct nbuf {
	conn_t *conn;
	char data[2048];
	int len;
};
typedef struct nbuf nbuf_t;

struct client {
	int charpos;
	nbuf_t out;
};

typedef struct client client_t;

char linechars[] =
	"!\"#$%&'()*+,-./0123456789:;<=>?@"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
	"abcdefghijklmnopqrstuvwxyz{|}~ ";

int main()
{
	char address[] = "0.0.0.0:1900";
	conn_t *l = net_listen("tcp", address);
	if(!l) {
		fatal("listen failed: %s", net_error());
	}
	logmsg("listening at %s", address);

	while(1) {
		conn_t *s = net_accept(l);
		if(!s) {
			err("accept error");
			continue;
		}

		client_t *c = calloc(1, sizeof(*c));
		c->out.conn = s;
		process_client(c);
		//th_detach(th_start(&process_client, c));
	}

	net_close(l);
	return 0;
}

void *process_client(client_t *c)
{
	conn_t *conn = c->out.conn;
	logmsg("%s connected", net_addr(conn));
	char buf[256] = {};

	while(1) {
		if(net_incoming(conn)) {
			if(net_read(conn, buf, sizeof(buf)) == 0) {
				break;
			}
			continue;
		}

		if(!send_line(c)) {
			err("send_line error");
			break;
		}
	}
	logmsg("%s disconnected", net_addr(conn));
	net_close(conn);
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
		if(net_write(b->conn, b->data, b->len) < b->len) {
			return EOF;
		}
		b->len = 0;
	}
	return ch;
}
