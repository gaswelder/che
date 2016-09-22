/*
 * Echo server for a single client
 */

import "net"
import "cli"
import "log"

int main()
{
	const char *addr = "0.0.0.0:7000";
	conn_t *l = net_listen("tcp", addr);
	if(!l) {
		fatal("listen failed: %s", net_error());
	}
	logmsg("listening at %s", addr);

	while(1) {
		conn_t *s = net_accept(l);
		if(!s) {
			err("accept error: %s", net_error());
			continue;
		}
		process_client(s);
	}

	net_close(l);
	return 0;
}

void *process_client(conn_t *c)
{
	logmsg("%s connected", net_addr(c));
	char buf[256];

	while(1) {
		int len = net_read(c, buf, sizeof(buf));
		if(len == -1) {
			err("read error");
			break;
		}
		if(len == 0) {
			break;
		}

		if(net_write(c, buf, len) < len) {
			err("write error");
			break;
		}
	}
	logmsg("%s disconnected", net_addr(c));
	net_close(c);
	return NULL;
}
