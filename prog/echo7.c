/*
 * Echo server for a single client
 */

#import os/net
#import cli
#import log
#import os/threads

int main()
{
	const char *addr = "0.0.0.0:7000";
	net_t *l = net_listen("tcp", addr);
	if(!l) {
		fatal("listen failed: %s", net_error());
	}
	logmsg("listening at %s", addr);

	while(1) {
		net_t *s = net_accept(l);
		if(!s) {
			err("accept error: %s", net_error());
			continue;
		}
		thr_t *t = thr_new(process_client, s);
		assert(t);
		thr_detach(t);
	}

	net_close(l);
	return 0;
}

void *process_client(void *arg)
{
	net_t *c = (net_t *) arg;
	logmsg("%s connected", net_addr(c));
	char buf[256] = {0};

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
