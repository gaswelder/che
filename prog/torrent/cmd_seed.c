#import files.c
#import formats/torrent
#import opt
#import tracker.c
#import rnd
#import error
#import os/net
#import peerproto.c
#import reader
#import writer
// #import os/threads

pub int run(int argc, char **argv) {
	opt.nargs(1, "<torrentfile>");
	opt.summary("Serves a torrent for others to download.");
	char **args = opt.parse(argc, argv);
	const char *path = args[0];

	torrent.info_t *tf = torrent.from_file(path);
	if (!tf) {
		fprintf(stderr, "failed to load %s: %s\n", path, strerror(errno));
		return 1;
	}

	printf("checking pieces\n");
	size_t npieces = torrent.npieces(tf);
	for (size_t i = 0; i < npieces; i++) {
		if (files.check_piece(".", tf, i)) {
			printf("%zu: OK\n", i);
		} else {
			printf("%zu: mismatch\n", i);
		}
	}

	seeder(tf);
	return 0;
}

const char *alpha = "0123456789";

void seeder(torrent.info_t *tf) {
	uint8_t peer_id[20];
	rnd.bytes(peer_id, 20);
	for (int i = 0; i < 20; i++) {
		peer_id[i] = alpha[peer_id[i] % 10];
	}

	error.t err = {};

	// Open a listener for peers.
	net.conn_t *l = net.listen("tcp", "localhost:6881", &err);
	if (err.set) {
		panic("%s", err.msg);
	}

	// Announce ourselves to the world.
	tracker.tracker_response_t *res = tracker.announce("localhost:8000", tf, peer_id, "6881", &err);
	if (err.set) {
		fprintf(stderr, "failed to connect to tracker: %s\n", err.msg);
		return;
	}
	tracker.print(res);

	
	// Wait for a peer to connect.
	puts("waiting for peers");
	net.conn_t *conn = net.accept(l, &err);
	if (err.set) {
		panic("%s", err.msg);
	}

	
	reader.t *r = net.getreader(conn);
	writer.t *w = net.getwriter(conn);

	// Handshake.
	peerproto.read_handshake1(r);
	peerproto.write_handshake(w, (uint8_t *) tf->infohash_bytes, peer_id);
	
	// Spawn a thread to talk to the peer.
	arg_t *a = calloc!(1, sizeof(arg_t));
	a->r = r;
	a->w = w;
	a->tf = tf;

	// threads.thr_t *thr = threads.start(runproto, a);
	// threads.wait(thr, &err);
	runproto(a);
	free(a);
	// if (err.set) {
	// 	panic("thread wait failed: %s", err.msg);
	// }

	// Hangup on the peer.
	net.close(conn);

	// Stop seeding.
	net.close(l);
}

typedef {
	reader.t *r;
	writer.t *w;
	torrent.info_t *tf;
} arg_t;

void *runproto(void *arg) {
	arg_t *a = arg;
	reader.t *r = a->r;
	writer.t *w = a->w;
	torrent.info_t *tf = a->tf;
	while (true) {
		peerproto.msg_t msg = {};
		peerproto.readmsg(r, w, &msg);
		switch (msg.id) {
			// The peer tells which pieces it has.
			case peerproto.MSG_BITFIELD: {
				bool *flags = (void *) msg.data;
				size_t npieces = torrent.npieces(tf);
				for (size_t i = 0; i < npieces; i++) {
					printf("peer has %zu: %d\n", i, flags[i]);
				}
			}
			// The peer requests that we send it a piece.
			case peerproto.MSG_REQUEST: {
				peerproto.msg_request_t *req = (void *) msg.data;
				printf("got read request: (%u, %u, %u)\n", req->index, req->begin, req->length);
				files.range_t freq = {
					.index = req->index,
					.begin = req->begin,
					.length = req->length,
				};
				printf("reading slice\n");
				uint8_t *tmpbuf1 = calloc!(32 * 1024, 1);
				files.readslice(".", tf, freq, tmpbuf1);
				puts("sending piece");
				if (peerproto.write_piece(w, freq, tmpbuf1) < 0) {
					puts("failed to send piece, peer likely disconnected");
					free(tmpbuf1);
					return NULL;
				}
				puts("ok");
				free(tmpbuf1);
			}
			default: { panic("unexpected message type"); }
		}
	}
	return NULL;
}
