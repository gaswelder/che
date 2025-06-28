#import formats/bencode
#import formats/torrent
#import ioloop.c
#import protocols/http
#import reader
#import writer

pub typedef {
	char id[20];
	char ip[100];
	int port;
} peer_entry_t;

pub typedef {
	int complete, incomplete;
	int interval;
	peer_entry_t peers[100];
	size_t peers_number;
} tracker_response_t;

torrent.info_t *_tf = NULL;
uint8_t *_peer_id = NULL;
tracker_response_t last_response = {};

pub bool init(torrent.info_t *tf, uint8_t *peer_id) {
	_tf = tf;
	_peer_id = peer_id;
	return true;
}

pub tracker_response_t *getstate() {
	return &last_response;
}

pub void process(void *ctx, int event, void *edata) {
	switch (event) {
		case ioloop.CONNECTED: {
			send_announce(ctx, NULL);
		}
		case ioloop.EXIT: {}
		case ioloop.DATA_IN: {
			ioloop.buff_t *b = edata;
			reader.t *re = reader.static_buffer((uint8_t *)b->data, b->len);
			http.response_t res = {};
			if (!http.parse_response(re, &res)) {
				panic("failed to parse tracker's HTTP response");
			}
			read_tracker_response(res.body, &last_response);
			reader.free(re);
			print();
		}
		case ioloop.WRITE_FINISHED: {}
		default: {
			panic("unknown event: %d", event);
		}
	}
}

void send_announce(void *ctx, const char *event) {
	http.request_t *req = http.newreq(http.GET, "/announce");
	http.reqparam(req, "info_hash", _tf->infohash_bytes, 20);
	http.reqparam(req, "peer_id", (char *)_peer_id, 20);
	http.reqparams(req, "port", "6881");

	// bytes uploaded and downloaded since the client
	// sent the 'started' event.
	http.reqparams(req, "uploaded", "1234");
	http.reqparams(req, "downloaded", "0");

	// number of bytes still to download to get full set.
	http.reqparams(req, "left", "0");

	// event
	// empty or not present: regular interval announcement
	// started: download begins
	// completed: the download is complete. don't send if it was already downloaded
	// stopped: ceased downloading
	if (event) {
		http.reqparams(req, "event", event);
	}

	uint8_t buf[1000] = {};
	writer.t *w = writer.static_buffer(buf, sizeof(buf));
	if (http.write_request(w, req) < 0) {
		panic("write_request failed");
	}
	http.freereq(req);
	writer.free(w);
	ioloop.write(ctx, (char *)buf, strlen((char *)buf));
}

pub void print() {
	tracker_response_t *resp = &last_response;
	printf("tracker state: complete=%d, incomplete=%d, interval=%d\n", resp->complete, resp->incomplete, resp->interval);
	for (size_t i = 0; i < resp->peers_number; i++) {
		peer_entry_t *peer = &resp->peers[i];
		if (!memcmp(peer->id, _peer_id, 20)) {
			printf("- us %zu: %s at %s:%d\n", i, peer->id, peer->ip, peer->port);
        } else {
			printf("- peer %zu: %s at %s:%d\n", i, peer->id, peer->ip, peer->port);
		}
	}
}

void read_tracker_response(char *body, tracker_response_t *resp) {
	bencode.reader_t *ber = bencode.newreader(body, 10000);
	char key[1000] = {};
	bencode.enter(ber);
	while (bencode.more(ber)) {
		bencode.key(ber, (uint8_t *)key, 1000);
		switch str (key) {
			case "complete": { resp->complete = bencode.readnum(ber); }
			case "incomplete": { resp->incomplete = bencode.readnum(ber); }
			case "interval": { resp->interval = bencode.readnum(ber); }
			case "peers": {
				bencode.enter(ber);
				while (bencode.more(ber)) {
					peer_entry_t *peer = &resp->peers[resp->peers_number++];
					read_peer(ber, peer);
				}
				bencode.leave(ber);
			}
			default: {
				panic("unknown key: %s", key);
			}
		}
	}
	bencode.leave(ber);
	bencode.freereader(ber);
}

void read_peer(bencode.reader_t *ber, peer_entry_t *p) {
	memset(p, 0, sizeof(peer_entry_t));
	char key[1000] = {};
	bencode.enter(ber);
	while (bencode.more(ber)) {
		bencode.key(ber, (uint8_t *)key, 1000);
		switch str (key) {
			case "ip": { bencode.readbuf(ber, (uint8_t*)&p->ip, sizeof(p->ip)); }
			case "peer id": { bencode.readbuf(ber, (uint8_t*)&p->id, sizeof(p->id)); }
			case "port": { p->port = bencode.readnum(ber); }
			default: { panic("unknown peer dict key: %s", key); }
		}
	}
	bencode.leave(ber);
}
