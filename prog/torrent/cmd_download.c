#import formats/torrent
#import opt
#import tracker.c
#import rnd
#import error
#import os/net
#import writer
#import peerproto.c
#import reader
#import files.c

const char *outdir = ".";
const char *tracker_addr = "localhost:8000";
const char *alpha = "0123456789";

pub int run(int argc, char **argv) {
	opt.nargs(1, "<torrentfile>");
	opt.summary("Downloads a torrent.");
	char **args = opt.parse(argc, argv);
	const char *path = args[0];

	// Load the torrent file.
	torrent.info_t *tf = torrent.from_file(path);
	if (!tf) {
		fprintf(stderr, "failed to load %s: %s\n", path, strerror(errno));
		return 1;
	}

	// Generate a peer ID for us.
	uint8_t peer_id[20];
	rnd.bytes(peer_id, 20);
	for (int i = 0; i < 20; i++) {
		peer_id[i] = alpha[peer_id[i] % 10];
	}

	// listen on 6882

	error.t err = {};
	tracker.tracker_response_t *resp = tracker.announce(tracker_addr, tf, peer_id, "6882", &err);
	if (err.set) {
		panic("%s", err.msg);
	}
	tracker.print(resp);

	tracker.peer_entry_t peer = choose_peer(resp, peer_id);
	printf("chose peer #%s at %s:%d\n", peer.id, peer.ip, peer.port);

	free(resp);

	// Connect to the peer.
	net.conn_t *conn = net.connect("tcp", "localhost:6881", &err);
	if (err.set) {
		panic("%s", err.msg);
	}

	process_conn(conn, tf, peer_id);

	net.close(conn);
	return 0;
}

tracker.peer_entry_t choose_peer(tracker.tracker_response_t *_state, uint8_t *peer_id) {
	(void) peer_id;
	tracker.peer_entry_t p = {};
	for (size_t i = 0; i < _state->peers_number; i++) {
		tracker.peer_entry_t *pi = &_state->peers[i];
		memcpy(&p, pi, sizeof(tracker.peer_entry_t));
	}
	return p;
}

void process_conn(net.conn_t *conn, torrent.info_t *tf, uint8_t *peer_id) {
	writer.t *w = net.getwriter(conn);
	reader.t *r = net.getreader(conn);

	// Handshake.
	peerproto.write_handshake(w, (uint8_t *) tf->infohash_bytes, peer_id);
	peerproto.read_handshake1(r);

	// peerproto.write_bitfield(w, tf);
	
	size_t n = torrent.npieces(tf);
	for (size_t i = 0; i < n; i++) {
		printf("downloading piece %zu / %zu\n", i, n);
		download_piece(r, w, i, tf);
		if (!files.check_piece(outdir, tf, i)) {
			panic("piece check failed");
		}
	}

	reader.free(r);
	writer.free(w);
}

void download_piece(reader.t *r, writer.t *w, size_t i, torrent.info_t *tf) {
	size_t piecelen = files.get_piece_length(tf, i);
	size_t slice_size = 16 * 1024;
	for (size_t pos = 0; pos < piecelen; pos += slice_size) {
		if (pos + slice_size > piecelen) {
			slice_size = piecelen - pos;
		}
		//
		// request the slice
		//
		files.range_t req = {
			.index = i,
			.begin = pos,
			.length = slice_size,
		};
		peerproto.write_request(w, req);

		//
		// wait for the slice
		//
		peerproto.msg_t msg = {};
		peerproto.readmsg(r, w, &msg);
		if (msg.id != peerproto.MSG_PIECE) {
			panic("unexpected message");
		}
		peerproto.msg_piece_t *piece = (void *) msg.data;
		files.range_t freq = {
			.index = piece->index,
			.begin = piece->begin,
			.length = piece->length,
		};
		files.writeslice(outdir, tf, freq, piece->data);
	}
}
