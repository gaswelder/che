#import crypt/sha1
#import formats/torrent
#import os/fs
#import opt

const size_t PIECE_LENGTH = 256 * 1024;

pub int run(int argc, char *argv[]) {
    opt.opt_summary("creates a torrent file");
    opt.nargs(2, "<file-to-share> <outfile>");
    char **args = opt.parse(argc, argv);
    const char *filepath = args[0];
    const char *outpath = args[1];

    torrent.info_t *info = newtorrent();

    if (!addfile(info, filepath)) {
        fprintf(stderr, "could not add file %s: %s\n", filepath, strerror(errno));
        return 1;
    }
    if (!writetorrent(info, outpath)) {
        fprintf(stderr, "could not write %s: %s\n", outpath, strerror(errno));
        return 1;
    }
    return 0;
}

torrent.info_t *newtorrent() {
    torrent.info_t *info = calloc(1, sizeof(torrent.info_t));
    if (!info) panic("failed to allocate memory");

    strcpy(info->announce, "http://localhost:8000/announce");
    strcpy(info->created_by, "");
    strcpy(info->comment, "");
    info->creation_date = time(NULL);
    info->piece_length = PIECE_LENGTH;
    return info;
}

bool addfile(torrent.info_t *info, const char *filepath) {
    const char *filename = fs.basename(filepath);
    strcpy(info->name, filename);

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "failed to open %s: %s\n", filepath, strerror(errno));
        return false;
    }
    uint8_t piece[PIECE_LENGTH];
    size_t piecelen = 0;
    size_t npieces = 0;

    while (!feof(f)) {
        int c = fgetc(f);
        if (c == EOF) break;
        if (piecelen == PIECE_LENGTH) {
            addpiece(info, piece, piecelen, npieces++);
            piecelen = 0;
        }
        piece[piecelen++] = c;
    }
    if (piecelen > 0) {
        addpiece(info, piece, piecelen, npieces++);
    }
    fclose(f);
    return true;
}

void addpiece(torrent.info_t *info, uint8_t *piece, size_t piecelen, size_t i) {
    info->pieces = realloc(info->pieces, info->piece_length * (i+1));
    if (!info->pieces) {
        panic("realloc failed");
    }
    sha1.digest_t hash = {};
    for (size_t j = 0; j < piecelen; j++) {
        sha1.add(&hash, piece[j]);
    }
    sha1.end(&hash);

    char buf[41] = {};
    sha1.format(&hash, buf, sizeof(buf));
    printf("piece %zu: %s\n", i, buf);

    uint8_t *hashesp = info->pieces + i * 20;
    sha1.as_bytes(&hash, hashesp);

    info->length += piecelen;
}

bool writetorrent(torrent.info_t *info, const char *outpath) {
    FILE *f = fopen(outpath, "wb");
    if (!f) return false;
    bool ok = torrent.writefile(f, info);
    fclose(f);
    return ok;
}
