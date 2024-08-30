#import formats/torrent
#import fs
#import strings
#import time

pub int cmd(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "arguments: <torrentfile>\n");
		return 1;
	}
	size_t size = 0;
	uint8_t *data = fs.readfile(argv[1], &size);
	if (!data) {
		fprintf(stderr, "failed to read file: %s\n", strerror(errno));
		return 1;
	}
	torrent.info_t *tf = torrent.parse(data, size);

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

	size_t total = 0;

	if (tf->nfiles == 0) {
		total = tf->length;
		size_t npieces = total/tf->piece_length + (total % tf->piece_length != 0);
		strings.fmt_bytes(total, tmp, 100);
		printf("length = %zu (%s), %zu pieces\n", tf->length, tmp, npieces);
	} else {
		for (size_t i = 0; i < tf->nfiles; i++) {
			total += tf->files[i].length;
			strings.fmt_bytes(tf->files[i].length, tmp, 100);
			printf("%zu: %10s\t%s\n", i, tmp, tf->files[i].path);
		}
		size_t npieces = total/tf->piece_length + (total % tf->piece_length != 0);
		strings.fmt_bytes(total, tmp, 100);
		printf("length = %zu (%s), %zu files, %zu pieces\n", total, tmp, tf->nfiles, npieces);
	}

	printf("infohash = %s\n", tf->infohash);


	// Prints piece hashes:
	// uint8_t *p = tf->pieces;
	// for (size_t n = 0; n < npieces; n++) {
	//	 printf("piece %zu: ", n);
	//	 for (int i = 0; i < 20; i++) {
	//		 printf("%02x", *p++);
	//	 }
	//	 printf("\n");
	// }

	torrent.free(tf);
	return 0;
}
