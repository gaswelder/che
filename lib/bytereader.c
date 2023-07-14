pub typedef {
    FILE *f;
} reader_t;

pub reader_t *newreader(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    reader_t *r = calloc(1, sizeof(reader_t));
    if (!r) {
        fclose(f);
        return NULL;
    }
    r->f = f;
    return r;
}

pub bool ended(reader_t *r) {
    return feof(r->f);
}

pub void freereader(reader_t *r) {
    fclose(r->f);
    free(r);
}

pub int readc(reader_t *r) {
    return fgetc(r->f);
}

// pub uint8_t read8(reader_t *r) {
//     return fgetc(r->f);
// }

pub uint16_t read16(reader_t *r) {
    uint16_t val = 0;
    for (int i = 0; i < 2; i++) {
        val = val * 256 + fgetc(r->f);
    }
    return val;
}

pub uint32_t read32(reader_t *r) {
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        val = val * 256 + fgetc(r->f);
    }
    return val;
}
