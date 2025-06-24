// Echo server, rfc862.

#import log
#import os/net
#import os/threads
#import reader

int main() {
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
		threads.thr_t *t = threads.start(process_client, s);
		threads.thr_detach(t);
	}

	net.close(l);
	return 0;
}

void *process_client(void *arg) {
	net.net_t *c = arg;
	log.logmsg("%s connected", net.net_addr(c));
	uint8_t buf[256] = {0};
	reader.t *r = net.getreader(c);

	while(1) {
		int len = reader.read(r, buf, sizeof(buf));
		if (len < 0) {
			fprintf(stderr, "read error: %s\n", strerror(errno));
			break;
		}
		if(len == 0) {
			break;
		}

		if (net.write(c, (char *)buf, len) < len) {
			fprintf(stderr, "write error: %s\n", strerror(errno));
			break;
		}
	}
	log.logmsg("%s disconnected", net.net_addr(c));
	reader.free(r);
	net.close(c);
	return NULL;
}
