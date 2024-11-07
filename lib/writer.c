
pub typedef int writefunc_t(void *, const char *, size_t); // ctx, data, len
pub typedef void freefunc_t(void *);

pub typedef {
	size_t nwritten;
	void *data;
	writefunc_t *write;
	freefunc_t *free;
} t;

// Writes n bytes from data to w.
// Returns the number of bytes written.
pub int write(t *w, const char *data, size_t n) {
	int r = w->write(w->data, data, n);
	if (r > 0) w->nwritten += r;
	return r;
}

pub void free(t *w) {
	if (w->free) w->free(w->data);
}

pub int writebyte(t *w, char b) {
	return write(w, &b, 1);
}


t *_stdout = NULL;

// Returns a global instance of a stdout writer.
// This function is not thread safe.
pub t *stdout() {
	if (!_stdout) {
		_stdout = file(OS.stdout);
	}
	return _stdout;
}

// Returns a writer to a file.
pub t *file(FILE *f) {
	t *w = calloc(1, sizeof(t));
	if (!w) panic("calloc failed");
	w->data = f;
	w->write = f_write;
	return w;
}

int f_write(void *ctx, const char *data, size_t n) {
	FILE *f = ctx;
	size_t r = fwrite(data, 1, n, f);
	return (int) r;
}


typedef { char *data; size_t pos, size; } st_t;

// Returns a writer to a static buffer.
// When the buffer's place runs out, write will return EOF.
pub t *static_buffer(char *data, size_t size) {
	t *w = calloc(1, sizeof(t));
	st_t *s = calloc(1, sizeof(st_t));
	if (!w || !s) panic("calloc failed");

	s->data = data;
	s->size = size;

	w->write = st_write;
	w->free = OS.free;
	w->data = s;
	return w;
}

int st_write(void *ctx, const char *data, size_t n) {
	st_t *w = ctx;
	if (w->pos >= w->size) {
		return EOF;
	}
	int r = 0;
	for (size_t i = 0; i < n; i++) {
		if (w->pos >= w->size) {
			break;
		}
		w->data[w->pos++] = data[i];
		r++;
	}
	return r;
}
