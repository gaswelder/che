#import formats/bencode
#import formats/torrent
#import protocols/http
#import reader
#import writer
#import os/net
#import error

pub typedef {
	char id[100];
	char ip[100];
	int port;
} peer_entry_t;

pub typedef {
	int complete;
	int incomplete;
	int interval;
	peer_entry_t peers[100];
	size_t peers_number;
} tracker_response_t;

// Writes an announce to the given tracker.
// Returns allocated tracker's response.
// The caller must free the response.
pub tracker_response_t *announce(const char *addr, torrent.info_t *_tf, uint8_t *_peer_id, const char *port, error.t *err) {
	// Connect to the tracker.
	net.conn_t *conn = net.connect("tcp", addr, err);
	if (err->set) {
		return NULL;
	}

	// Write the announce.
	writer.t *w = net.getwriter(conn);
	reader.t *re = net.getreader(conn);
	tracker_response_t *res = write_announce(w, re, _tf, _peer_id, port);
	reader.free(re);
	writer.free(w);

	// Disconnect and return the response.
	net.close(conn);
	return res;
}

tracker_response_t *write_announce(writer.t *w, reader.t *re, torrent.info_t *_tf, uint8_t *_peer_id, const char *port) {
	http.request_t *req = http.newreq(http.GET, "/announce");
	// event
	// empty or not present: regular interval announcement
	// "started": download begins
	// "completed": the download is complete. don't send if it was already downloaded
	// "stopped": ceased downloading
	const char *event = NULL;
	if (event) {
		http.reqparams(req, "event", event);
	}
	http.reqparam(req, "info_hash", _tf->infohash_bytes, 20);
	http.reqparam(req, "peer_id", (char *) _peer_id, 20);
	http.reqparams(req, "port", port);
	http.reqparams(req, "uploaded", "1234"); // bytes uploaded since "started"
	http.reqparams(req, "downloaded", "0"); // bytes downloaded since "started"
	http.reqparams(req, "left", "0"); // bytes yet to download

	if (http.write_request(w, req) < 0) {
		panic("write_request failed");
	}
	http.freereq(req);	

	//
	// read and process data
	//
	http.response_t res = {};
	if (!http.parse_response(re, &res)) {
		panic("failed to parse tracker's HTTP response");
	}

	// Parse and return the response.
	tracker_response_t *last_response = calloc!(1, sizeof(tracker_response_t));
	read_tracker_response(res.body, last_response);
	return last_response;
}


pub void print(tracker_response_t *resp) {
	printf("tracker state: complete=%d, incomplete=%d, interval=%d\n", resp->complete, resp->incomplete, resp->interval);
	for (size_t i = 0; i < resp->peers_number; i++) {
		peer_entry_t *peer = &resp->peers[i];
		printf("- peer %zu: %s at %s:%d\n", i, peer->id, peer->ip, peer->port);
	}
}

void read_tracker_response(char *body, tracker_response_t *resp) {
	bencode.reader_t *ber = bencode.newreader(body, 10000);
	char key[1000] = {};
	bencode.enter(ber);
	while (bencode.more(ber)) {
		bencode.key(ber, (uint8_t *) key, 1000);
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
			default: { panic("unknown key: %s", key); }
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
		bencode.key(ber, (uint8_t *) key, 1000);
		switch str (key) {
			case "ip": { bencode.readbuf(ber, (uint8_t *) &p->ip, sizeof(p->ip)); }
			case "peer id": { bencode.readbuf(ber, (uint8_t *) &p->id, sizeof(p->id)); }
			case "port": { p->port = bencode.readnum(ber); }
			default: { panic("unknown peer dict key: %s", key); }
		}
	}
	bencode.leave(ber);
}
