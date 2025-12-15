pub typedef {
	FILE *f;
	char *buf;
	size_t bufsize;
	bool ended;
} t;

// Allocates and returns a new instance reading from f.
pub t *new(FILE *f) {
	t *r = calloc!(1, sizeof(t));
	r->f = f;
	r->bufsize = 4096;
	r->buf = calloc!(r->bufsize, 1);
	return r;
}

// Reads the next line into b.
// Returns false if there is nothing more to read.
pub bool read(t *b) {
	if (b->ended) {
		return false;
	}
	size_t len = 0;
	while (true) {
		char c = fgetc(b->f);
		if (c == EOF) {
			b->ended = true;
			break;
		}
		if (len + 2 > b->bufsize) {
			b->bufsize *= 2;
			b->buf = realloc(b->buf, b->bufsize);
			if (!b->buf) {
				panic("failed to allocate memory");
			}
		}
		b->buf[len++] = c;
		if (c == '\n') break;
	}
	b->buf[len] = '\0';
	return len > 0;
}

// Returns a pointer to the memory with the line.
pub char *line(t *b) {
	return b->buf;
}

// Frees the instance.
pub void free(t *b) {
	OS.free(b->buf);
	OS.free(b);
}
