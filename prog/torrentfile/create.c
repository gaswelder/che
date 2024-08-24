#import crypt/sha1
#import formats/torrent
#import fs

const int PIECE_LENGTH = 256 * 1024;

pub int cmd(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "arguments: <file>\n");
        return 1;
    }
    const char *filepath = argv[1];
    torrent.info_t info = {};
    strcpy(info.announce, "https://foo.bar");
    strcpy(info.created_by, "kektorrent");
    strcpy(info.comment, "no comment");
    info.creation_date = time(NULL);

    if (!addfile(&info, filepath)) {
        fprintf(stderr, "addfile failed\n");
        return 1;
    }
    const char *path = "out.torrent";
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return 1;
    }
    if (!torrent.writefile(f, &info)) {
        fprintf(stderr, "writefile failed\n");
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

bool addfile(torrent.info_t *info, const char *filepath) {
    size_t filesize;
    if (!fs.filesize(filepath, &filesize)) {
        return false;
    }
    info->length = filesize;
    info->piece_length = PIECE_LENGTH;
    size_t npieces = filesize / PIECE_LENGTH;
    size_t lastlen = filesize % PIECE_LENGTH;
    if (lastlen) npieces++;
    printf("%zu pieces, last %zu bytes\n", npieces, lastlen);

    const char *filename = fs.basename(filepath);
    strcpy(info->name, filename);

    info->pieces = calloc(npieces, 20);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "failed to open %s: %s\n", filepath, strerror(errno));
        return 1;
    }
    uint8_t piece[PIECE_LENGTH];
    size_t piecelen = PIECE_LENGTH;
    uint8_t *hashesp = info->pieces;
    for (size_t i = 0; i < npieces; i++) {
        // Read the next piece.
        if (i == npieces-1 && lastlen) piecelen = lastlen;
        int r = fread(piece, 1, piecelen, f);
        if ((size_t) r != piecelen) {
            panic("fread %zu/%zu -> %d", i, npieces, r);
        }

        // hash piece
        sha1.digest_t hash = {};
        for (size_t j = 0; j < piecelen; j++) {
            sha1.add(&hash, piece[j]);
        }
        sha1.end(&hash);

        char buf[41] = {};
        sha1.format(&hash, buf, sizeof(buf));
        printf("piece %zu/%zu: %s\n", i, npieces, buf);

        // append hash
        sha1.as_bytes(&hash, hashesp);
        hashesp += 20;
    }
    fclose(f);
    return true;
}
