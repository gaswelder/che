// *.torrent format

#import formats/bencode
#import formats/bencode_writer

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

/*
    pieces
    // multi mode:
    // A list of dictionaries each corresponding to a file.
    file_lengths;
    file_paths;
    files array of {length, path}
*/
    // ---------- end of info section -------------
} info_t;

pub void free(info_t *tf) {
    OS.free(tf->pieces);
    OS.free(tf);
}

pub info_t *parse(const uint8_t *data, size_t size) {
    info_t *tf = calloc(1, sizeof(info_t));
    if (!tf) return NULL;

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
            parse_info(r, tf);
        } else {
            panic("unknown meta key: %s (%c)\n", k, bencode.type(r));
        }
    }
    bencode.leave(r);
    bencode.freereader(r);
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
        } else {
            panic("UNKNOWN info[%s] = %c\n", k, bencode.type(r));
        }
    }
    bencode.leave(r);
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
