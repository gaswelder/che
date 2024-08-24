enum {
	FSTREAM = 'f',
	BUF = 'b'
};

pub typedef {
	int type;

	// for type == file
	FILE *f;

	// for type == buf
	uint8_t *buf;
	size_t bufsize;
	size_t pos;
} t;

pub t *tobuf(uint8_t *buf, size_t bufsize) {
	t *w = calloc(1, sizeof(t));
	if (!w) return NULL;
	w->buf = buf;
	w->bufsize = bufsize;
	w->type = BUF;
	return w;
}

pub t *tofile(FILE *f) {
	t *w = calloc(1, sizeof(t));
	if (!w) return NULL;
	w->f = f;
	w->type = FSTREAM;
	return w;
}

pub void free(t *w) {
	OS.free(w);
}

pub bool byte(t *w, uint8_t b) {
	switch (w->type) {
		case BUF: {
			if (w->pos >= w->bufsize) return false;
			w->buf[w->pos++] = b;
			return true;
		}
		case FSTREAM: {
			return fputc(b, w->f) != EOF;
		}
	}
	panic("unknown type");
}
