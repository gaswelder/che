#import files.c
#import formats/torrent
#import opt
#import os/ioloop
#import peer.c
#import tracker.c

pub int run(int argc, char **argv) {
    opt.opt_summary("seeds a torrent");
    opt.nargs(1, "<torrentfile>");
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

    // Generate a peer ID.
    uint8_t peer_id[20];
    for (int i = 0; i < 20; i++) {
		peer_id[i] = '0' + (rand() % ('z'-'0'));
	}

    peer.init(tf, peer_id);
	tracker.init(tf, peer_id);
    ioloop.listen("localhost:6881", peer.handle);
    ioloop.connect("localhost:8000", tracker.process, NULL);

	while (true) {
		if (!ioloop.process()) {
			fprintf(stderr, "no connections to process\n");
			return 1;
		}
	}
    return 0;
}
