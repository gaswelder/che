pub typedef {
    FILE *f;
} reader_t;

pub reader_t *newreader(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    reader_t *r = calloc!(1, sizeof(reader_t));
    r->f = f;
    return r;
}

pub bool ended(reader_t *r) {
    return feof(r->f) || ferror(r->f);
}

pub void freereader(reader_t *r) {
    fclose(r->f);
    free(r);
}

pub int readc(reader_t *r) {
	int c = fgetc(r->f);
	// printf(" (0x%X) ", c);
	return c;
}

pub int peekc(reader_t *r) {
	int c = fgetc(r->f);
	ungetc(c, r->f);
	return c;
}

// Big-endian
pub uint16_t read16(reader_t *r) {
	int a = readc(r);
	int b = readc(r);
    // uint8_t a = 0;
    // uint8_t b = 0;
    // fread(&a, sizeof(uint8_t), 1, r->f);
    // fread(&b, sizeof(uint8_t), 1, r->f);
    return a * 256 + b;
}

// Big-endian
pub uint32_t read32(reader_t *r) {
	int val = readc(r);
	val = val * 256 + readc(r);
	val = val * 256 + readc(r);
	val = val * 256 + readc(r);
	return val;
	// int a = readc(r);
	// int b = readc(r);
	// int c = readc(r);
	// int d = readc(r);

    // uint32_t val = 0;
    // for (int i = 0; i < 4; i++) {
    //     val = val * 256 + fgetc(r->f);
    // }
    // return val;
}
