/*
 * Echo server for a single client
 */

#import os/net
#import log
#import os/threads

int main()
{
	const char *addr = "0.0.0.0:7000";
	net.net_t *l = net.net_listen("tcp", addr);
	if(!l) {
		panic("listen failed: %s", net.net_error());
	}
	log.logmsg("listening at %s", addr);

	while(1) {
		net.net_t *s = net.net_accept(l);
		if(!s) {
			fprintf(stderr, "accept error: %s\n", net.net_error());
			continue;
		}
		threads.thr_t *t = threads.thr_new(process_client, s);
		assert(t);
		threads.thr_detach(t);
	}

	net.net_close(l);
	return 0;
}

void *process_client(void *arg)
{
	net.net_t *c = arg;
	log.logmsg("%s connected", net.net_addr(c));
	char buf[256] = {0};

	while(1) {
		int len = net.readconn(c, buf, sizeof(buf));
		if (len == -1) {
			fprintf(stderr, "read error: %s\n", strerror(errno));
			break;
		}
		if(len == 0) {
			break;
		}

		if(net.net_write(c, buf, len) < len) {
			fprintf(stderr, "write error: %s\n", strerror(errno));
			break;
		}
	}
	log.logmsg("%s disconnected", net.net_addr(c));
	net.net_close(c);
	return NULL;
}
