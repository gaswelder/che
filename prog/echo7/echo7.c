// Echo server, rfc862.
#import error
#import log
#import os/net
#import os/threads
#import reader

int main() {
	const char *addr = "0.0.0.0:7000";
	error.t err = {};
	net.net_t *l = net.net_listen("tcp", addr, &err);
	if (!l) {
		fprintf(stderr, "listen failed: %s\n", err.msg);
		return 1;
	}
	log.logmsg("listening at %s", addr);

	while (true) {
		net.net_t *s = net.net_accept(l, &err);
		if (!s) {
			fprintf(stderr, "accept error: %s\n", err.msg);
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
	uint8_t buf[256] = {};
	reader.t *r = net.getreader(c);

	while (true) {
		int len = reader.read(r, buf, sizeof(buf));
		if (len < 0) {
			fprintf(stderr, "read error: %s\n", strerror(errno));
			break;
		}
		if (len == 0) {
			break;
		}

		if (net.write(c, (char *) buf, len) < len) {
			fprintf(stderr, "write error: %s\n", strerror(errno));
			break;
		}
	}
	log.logmsg("%s disconnected", net.net_addr(c));
	reader.free(r);
	net.close(c);
	return NULL;
}
