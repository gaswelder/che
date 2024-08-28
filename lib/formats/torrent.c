// *.torrent format

#import formats/bencode
#import formats/bencode_writer
#import crypt/sha1

pub typedef {
	size_t length;
	char path[200];
} file_t;

pub typedef {
	// Tracker URL.
	char announce[200];

	// Free text.
	char created_by[200];

	// Creation date, UNIX seconds.
	int creation_date;

	// Free text.
	char comment[200];

	// ------------ info section -------------

	// Filename in single-file variant, or directory name in multi-file variant.
	// UTF-8.
	char name[200];

	// Number of bytes per piece, typically 256 KB.
	size_t piece_length;

	// Concatenation of each piece's SHA-1.
	// In multi-file variant the pieces are formed by concatenating the files
	// in the listing order.
	uint8_t *pieces;

	// Size of the file in bytes in single-file variant, 0 in multi-file variant.
	size_t length;

	// A list of {length, path} items, each corresponding to a file in the multi-file variant.
	file_t *files;
	size_t nfiles;
} info_t;

pub void free(info_t *tf) {
	OS.free(tf->pieces);
	if (tf->files) OS.free(tf->files);
	OS.free(tf);
}

pub info_t *parse(const uint8_t *data, size_t size) {
	info_t *tf = calloc(1, sizeof(info_t));
	if (!tf) return NULL;

	size_t info_begin = 0;
	size_t info_end = 0;

	uint8_t buf[1000] = {};
	bencode.reader_t *r = bencode.newreader(data, size);
	bencode.enter(r);
	while (bencode.more(r)) {
		bencode.key(r, buf, sizeof(buf));
		char *k = (char *)buf;
		if (strcmp(k, "announce") == 0) {
			bencode.readbuf(r, (uint8_t *) tf->announce, sizeof(tf->announce));
		} else if (strcmp(k, "creation date") == 0) {
			tf->creation_date = bencode.readnum(r);
		} else if (strcmp(k, "created by") == 0) {
			bencode.readbuf(r, (uint8_t *) tf->created_by, sizeof(tf->created_by));
		} else if (strcmp(k, "comment") == 0) {
			bencode.readbuf(r, (uint8_t *) tf->comment, sizeof(tf->comment));
		} else if (strcmp(k, "announce-list") == 0) {
			bencode.skip(r);
		} else if (strcmp(k, "info") == 0) {
			info_begin = bencode.pos(r);
			parse_info(r, tf);
			info_end = bencode.pos(r);
		} else if (strcmp(k, "encoding") == 0) {
			// uTorrent puts "UTF-8" there.
			bencode.skip(r);
		} else {
			fprintf(stderr, "unknown meta key: %s (%c)\n", k, bencode.type(r));
			bencode.skip(r);
		}
	}
	bencode.leave(r);
	bencode.freereader(r);

	sha1.digest_t hash = {};
	char hex[41] = {};
	for (size_t i = info_begin; i < info_end; i++) {
		sha1.add(&hash, data[i]);
	}
	sha1.end(&hash);
	sha1.format(&hash, hex, sizeof(hex));
	printf("infohash: %s\n", hex);
	return tf;
}

void parse_info(bencode.reader_t *r, info_t *tf) {
	bencode.enter(r);
	uint8_t buf[1000] = {};
	while (bencode.more(r)) {
		bencode.key(r, buf, sizeof(buf));
		char *k = (char *) buf;
		if (strcmp(k, "name") == 0) {
			bencode.readbuf(r, (uint8_t *) tf->name, sizeof(tf->name));
		} else if (strcmp(k, "piece length") == 0) {
			tf->piece_length = bencode.readnum(r);
		} else if (strcmp(k, "length") == 0) {
			tf->length = bencode.readnum(r);
		} else if (strcmp(k, "pieces") == 0) {
			size_t n = bencode.strsize(r);
			tf->pieces = calloc(n, 1);
			if (!tf->pieces) {
				panic("failed to allocate %zu bytes", n);
			}
			bencode.readbuf(r, tf->pieces, n);
		} else if (strcmp(k, "files") == 0) {
			bencode.enter(r);
			while (bencode.more(r)) {
				parse_file(r, tf);
			}
			bencode.leave(r);
		} else {
			panic("UNKNOWN info[%s] = %c\n", k, bencode.type(r));
		}
	}
	bencode.leave(r);
}

void parse_file(bencode.reader_t *r, info_t *tf) {
	uint8_t buf[1000] = {};
	const char *k = (char *) buf;

	allocfiles(tf);
	file_t *f = &tf->files[tf->nfiles++];

	bencode.enter(r);
	while (bencode.more(r)) {
		bencode.key(r, buf, sizeof(buf));
		if (strcmp(k, "path") == 0) {
			readpath(r, f->path, sizeof(f->path));
		} else if (strcmp(k, "length") == 0) {
			f->length = bencode.readnum(r);
		} else {
			panic("unknown file entry key: %s", k);
		}
	}
	bencode.leave(r);
}

void readpath(bencode.reader_t *r, char *buf, size_t bufsize) {
	memset(buf, 0, bufsize);
	char tmp[1000] = {};

	// path is a list "a", "b", "c.foo" for "a/b/c.foo".
	bencode.enter(r);
	bool first = true;
	while (bencode.more(r)) {
		bencode.readbuf(r, (uint8_t *)tmp, sizeof(tmp));
		if (first) {
			if (strlen(buf) + strlen(tmp) > bufsize) {
				panic("buffer too small for the path");
			}
			strcat(buf, tmp);
		} else {
			if (strlen(buf) + strlen(tmp) + 1 > bufsize) {
				panic("buffer too small for the path");
			}
			strcat(buf, "/");
			strcat(buf, tmp);
		}
		first = false;
	}
	bencode.leave(r);
}

void allocfiles(info_t *tf) {
	switch (tf->nfiles) {
		case 10000: { panic("too many files"); }
		case 1000, 100, 10: {
			tf->files = realloc(tf->files, tf->nfiles * 10 * sizeof(file_t));
			if (!tf->files) panic("realloc failed");
		}
		case 0: {
			tf->files = realloc(tf->files, 10 * sizeof(file_t));
			if (!tf->files) panic("realloc failed");
		}
	}
}

pub bool writefile(FILE *f, info_t *info) {
	size_t npieces = info->length / info->piece_length;
	size_t lastlen = info->length % info->piece_length;
	if (lastlen) npieces++;

	bencode_writer.t *w = bencode_writer.tofile(f);
	if (!w) return false;

	bencode_writer.begin(w, 'd');
	writestr(w, "announce"); writestr(w, info->announce);
	writestr(w, "comment"); writestr(w, info->comment);
	writestr(w, "created by"); writestr(w, info->created_by);
	writestr(w, "creation date"); bencode_writer.num(w, info->creation_date);
	writestr(w, "info");
	bencode_writer.begin(w, 'd');
	writestr(w, "length"); bencode_writer.num(w, info->length);
	writestr(w, "name"); writestr(w, info->name);
	writestr(w, "piece length"); bencode_writer.num(w, info->piece_length);
	writestr(w, "pieces"); bencode_writer.buf(w, info->pieces, npieces * 20);
	bencode_writer.end(w);
	bencode_writer.end(w);
	return true;
}

void writestr(bencode_writer.t *w, const char *s) {
	bencode_writer.buf(w, (uint8_t *) s, strlen(s));
}
