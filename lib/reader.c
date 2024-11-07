pub typedef void freefunc_t(void *);
pub typedef bool morefunc_t(void *);
pub typedef int readfunc_t(void *, uint8_t *, size_t);

pub typedef {
	void *data;
	size_t pos;
	freefunc_t *free;
	morefunc_t *more;
	readfunc_t *read;
} t;

typedef {
	const uint8_t *s;
	size_t pos;
	size_t len;
} str_t;

pub t *static_buffer(const uint8_t *data, size_t n) {
	t *r = calloc(1, sizeof(t));
	str_t *s = calloc(1, sizeof(str_t));
	if (!r || !s) panic("calloc failed");
	r->data = s;
	r->free = OS.free;
	r->more = st_more;
	r->read = st_read;
	s->s = data;
	s->len = n;
	return r;
}

pub t *stdin() {
	t *r = calloc(1, sizeof(t));
	if (!r) return NULL;
	r->more = io_more;
	r->read = io_read;
	return r;
}

pub t *string(const char *s) {
	return static_buffer((uint8_t *)s, strlen(s));
}

// Reads up to n bytes from r to buf.
// Returns the number of bytes read or a negative value for EOF or an error.
// Zero return means no bytes to read at the moment.
pub int read(t *reader, uint8_t *buf, size_t n) {
	int r = reader->read(reader->data, buf, n);
	if (r > 0) reader->pos += r;
	return r;
}

// Returns true if the reader hasn't been closed.
// It does not guarantee that a read will return a byte.
pub bool more(t *reader) {
	return reader->more(reader->data);
}

// Frees the reader.
pub void free(t *reader) {
	if (reader->free) reader->free(reader->data);
}

bool st_more(void *ctx) {
	str_t *data = ctx;
	return data->pos < data->len;
}

int st_read(void *ctx, uint8_t *buf, size_t n) {
	str_t *s = ctx;
	if (s->pos >= s->len) return EOF;
	int r = 0;
	for (size_t i = 0; i < n; i++) {
		if (s->pos == s->len) break;
		if (buf) buf[i] = s->s[s->pos];
		s->pos++;
		r++;
	}
	return r;
}

bool io_more(void *ctx) {
	(void) ctx;
	return !feof(OS.stdin) && !ferror(OS.stdin);
}

int io_read(void *ctx, uint8_t *buf, size_t n) {
	(void) ctx;
	size_t r = fread(buf, 1, n, OS.stdin);
	if (r == 0) {
		if (feof(OS.stdin) || ferror(OS.stdin)) return EOF;
	}
	return (int) r;
}
