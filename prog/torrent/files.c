#import clip/vec
#import crypt/sha1
#import dbg
#import formats/torrent
#import lib.c
#import strings

const char *DBG_TAG = "files";

// All files in a torrent are glued together and then split into equal
// pieces (except the last one, of course).
// This is a slice in terms of the protocol's pieces.
pub typedef {
    uint32_t index; // piece index
    uint32_t begin; // offset in the piece
    uint32_t length; // how many bytes
} range_t;

// The protocol deals with pieces, but on disk we deal with files.
// So piece ranges are converted to file ranges.
// This is a slice in terms of files.
pub typedef {
    char path[1000];
    size_t begin; // offset in the file
    size_t length; // how many bytes
} file_slice_t;

pub typedef {
    char path[1000];
    size_t beginpos;
    size_t endpos;
} file_t;

// Convert a protocol's piece slice to a list of corresponding file slices.
vec.t *map(torrent.info_t *tf, range_t s) {
    vec.t *l = vec.new(sizeof(file_slice_t));

    // Convert the slice s to torrent-global coordinates: [a, b).
    size_t a = s.index * tf->piece_length + s.begin;
    size_t b = a + s.length;

    vec.t *files = get_file_list(tf);
    size_t n = vec.len(files);
    for (size_t i = 0; i < n; i++) {
        file_t *f = vec.index(files, i);
        size_t f1 = f->beginpos;
        size_t f2 = f->endpos;
        if (f2 <= a || f1 >= b) continue;

        size_t af = lib.maxsz(f1, a);
        size_t bf = lib.minsz(f2, b);

        file_slice_t *x = vec.push(l);
        strcpy(x->path, f->path);
        x->begin = af - f1;
        x->length = bf - af;
    }

    vec.free(files);
    return l;
}

// Returns list of files, each file with its begin and
// end position in the torrent.
pub vec.t *get_file_list(torrent.info_t *tf) {
    vec.t *l = vec.new(sizeof(file_t));

    if (tf->length > 0) {
        // single file
        file_t *f = vec.push(l);
        strcpy(f->path, tf->name);
        f->endpos = tf->length;
        return l;
    }

    size_t pos = 0;
    for (size_t i = 0; i < tf->nfiles; i++) {
        file_t *f = vec.push(l);
        strcpy(f->path, tf->name);
        strcat(f->path, "/");
        strcat(f->path, tf->files[i].path);
        f->beginpos = pos;
        pos += tf->files[i].length;
        f->endpos = pos;
    }
    return l;
}

// Writes a piece slice to disk in directory prefix.
pub void writeslice(const char *dirpath, torrent.info_t *tf, range_t bs, uint8_t *data) {
    dbg.m(DBG_TAG, "writing at %s: piece #%zu, range %zu + %zu", dirpath, bs.index, bs.begin, bs.length);
    if (sliceop(0, dirpath, tf, bs, (char *)data) < 0) {
        panic("sliceop failed");
    }
}

// Reads a piece slice from disk in directory dirpath.
pub void readslice(const char *dirpath, torrent.info_t *tf, range_t bs, uint8_t *buf) {
    dbg.m(DBG_TAG, "reading at %s: piece #%zu, range %zu + %zu", dirpath, bs.index, bs.begin, bs.length);
    if (sliceop(1, dirpath, tf, bs, (char *)buf) < 0) {
        panic("sliceop failed");
    }
}

int sliceop(int op, const char *prefix, torrent.info_t *tf, range_t bs, char *buf) {
    vec.t *locs = map(tf, bs);
    size_t n = vec.len(locs);
    int c = 0;
    for (size_t i = 0; i < n; i++) {
        file_slice_t *s = vec.index(locs, i);
        char *path = strings.newstr("%s/%s", prefix, s->path);
        int r;
        if (op == 1) {
            r = _read(path, s, buf);
        } else {
            r = _write(path, s, buf);
        }
        free(path);
        if (r < 0) {
            c = -1;
            break;
        }
        c += r;
    }
    vec.free(locs);
    return c;
}

int _write(const char *path, file_slice_t *s, char *buf) {
    dbg.m(DBG_TAG, "= writing file %s, range %zu + %zu", path, s->begin, s->length);
    FILE *f = fopen(path, "r+b");
    if (!f) f = fopen(path, "w+b");
    if (!f) return -1;
    if (fseek(f, s->begin, SEEK_SET) < 0) return -1;
    dbg.m(DBG_TAG, "fseek %zu -> %d", s->begin, ftell(f));
    size_t r = fwrite(buf, 1, s->length, f);
    dbg.m(DBG_TAG, "fwrite: expected %zu, got %zu", s->length, r);
    fclose(f);
    return (int) r;
}

int _read(const char *path, file_slice_t *s, char *buf) {
    dbg.m(DBG_TAG, "= reading file %s, range %zu + %zu", path, s->begin, s->length);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, s->begin, SEEK_SET) < 0) return -1;
    size_t r = fread(buf, 1, s->length, f);
    dbg.m(DBG_TAG, "fread: expected %lu, got %zu\n", s->length, r);
    fclose(f);
    return (int) r;
}

pub bool check_piece(const char *prefix, torrent.info_t *tf, size_t piece_index) {
    uint8_t hash1[20];
    torrent.piecehash(tf, piece_index, hash1);

    //
    // read the piece
    //
    uint8_t *piece = calloc(tf->piece_length, 1);
    size_t piecelen = get_piece_length(tf, piece_index);
    range_t bs = {
        .index = piece_index,
        .begin = 0,
        .length = piecelen
    };
    readslice(prefix, tf, bs, piece);
    // ----------------
    
    //
    // hash the piece
    //
    uint8_t hash2[20];
    sha1.digest_t digest = {};
	for (size_t i = 0; i < piecelen; i++) {
		sha1.add(&digest, piece[i]);
	}
	sha1.end(&digest);
	sha1.as_bytes(&digest, hash2);
    //

    free(piece);
    return memcmp(hash1, hash2, 20) == 0;
}

// Returns the length in bytes of the piece with index i.
pub size_t get_piece_length(torrent.info_t *tf, size_t i) {
    size_t n = torrent.npieces(tf);
    size_t piecelen = tf->piece_length;
    if (i == n-1) piecelen = torrent.lastlen(tf);
    return piecelen;
}
