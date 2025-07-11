#import formats/torrent
#import lib.c
#import opt
#import ioloop.c
#import peer.c
#import tracker.c

pub int run(int argc, char **argv) {
    opt.nargs(1, "<torrentfile>");
	opt.summary("Downloads a torrent.");
    char **args = opt.parse(argc, argv);
    const char *path = args[0];

    torrent.info_t *tf = torrent.from_file(path);
    if (!tf) {
        fprintf(stderr, "failed to load %s: %s\n", path, strerror(errno));
        return 1;
    }

    uint8_t peer_id[20];
	lib.gen_id(peer_id);

    peer.init(tf, peer_id);
	tracker.init(tf, peer_id);
    ioloop.listen("localhost:6881", peer.handle);
    ioloop.connect("localhost:8000", tracker.process, NULL);

    bool started = false;
    while (true) {
		if (!ioloop.process()) {
			fprintf(stderr, "no connections to process\n");
			return 1;
		}
        if (!started && tracker.getstate()->peers_number > 0) {
			started = true;
            tracker.peer_entry_t p = choose_peer(peer_id);
			char addr[1000] = {};
			sprintf(addr, "%s:%d", p.ip, p.port);
			printf("choosing peer %s\n", addr);
            peer.init_t pi = { .foo = 123 };
			ioloop.connect(addr, peer.handle, &pi);
		}
	}
    return 0;
}

const bool TEST = true;

tracker.peer_entry_t choose_peer(uint8_t *peer_id) {
    tracker.peer_entry_t p = {};
    tracker.tracker_response_t *_state = tracker.getstate();
    for (size_t i = 0; i < _state->peers_number; i++) {
        tracker.peer_entry_t *pi = &_state->peers[i];
        bool is_us = !memcmp(pi->id, peer_id, 20);
        // Choose ourselves for testing.
        if (TEST) {
            if (is_us) {
                memcpy(&p, pi, sizeof(tracker.peer_entry_t));
                break;
            }
        } else {
            if (!is_us && !strcmp(pi->ip, "127.0.0.1")) {
                memcpy(&p, pi, sizeof(tracker.peer_entry_t));
                break;
            }
        }
        
    }
    return p;
}
