#import clip/vec
#import files.c
#import formats/torrent
#import opt
#import strings
#import time

pub int run(int argc, char **argv) {
	opt.nargs(1, "<torrent-file>");
	char **args = opt.parse(argc, argv);

    torrent.info_t *tf = torrent.from_file(args[0]);
	if (!tf) {
		fprintf(stderr, "failed to load torrent file: %s\n", strerror(errno));
		return 1;
	}
	printtorrent(tf);

    vec.t *l = files.get_file_list(tf);
    for (size_t i = 0; i < l->len; i++) {
        files.file_t *f = vec.index(l, i);
        printf("%zu: %10zu .. %-10zu %s\n", i, f->beginpos, f->endpos, f->path);
    }
    return 0;
}

void printtorrent(torrent.info_t *tf) {
    char tmp[100] = {};

    printf("announce = %s\n", tf->announce);

	time.t t = time.from_unix(tf->creation_date);
	time.format(t, time.knownformat(time.FMT_FOO), tmp, 100);
	printf("creation date = %d (%s)\n", tf->creation_date, tmp);
	printf("created by = %s\n", tf->created_by);
	printf("comment = %s\n", tf->comment);
	printf("name = %s\n", tf->name);

	strings.fmt_bytes(tf->piece_length, tmp, 100);
	printf("piece length = %zu (%s)\n", tf->piece_length, tmp);

	size_t total = torrent.totalsize(tf);
	size_t npieces = torrent.npieces(tf);
	strings.fmt_bytes(total, tmp, 100);

	if (tf->nfiles == 0) {
		printf("length = %zu (%s), %zu pieces\n", tf->length, tmp, npieces);
	} else {
		printf("length = %zu (%s), %zu files, %zu pieces\n", total, tmp, tf->nfiles, npieces);
	}

	printf("infohash = %s\n", tf->infohash);
}
