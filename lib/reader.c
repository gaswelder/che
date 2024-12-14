#include <unistd.h>

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
} membuf_t;

pub t *static_buffer(const uint8_t *data, size_t n) {
	t *r = calloc(1, sizeof(t));
	membuf_t *s = calloc(1, sizeof(membuf_t));
	if (!r || !s) panic("calloc failed");
	r->data = s;
	r->free = OS.free;
	r->more = mem_more;
	r->read = mem_read;
	s->s = data;
	s->len = n;
	return r;
}

pub t *file(FILE *f) {
	t *r = calloc(1, sizeof(t));
	if (!r) return NULL;
	r->data = f;
	r->more = file_more;
	r->read = file_read;
	return r;
}

pub t *stdin() {
	return file(OS.stdin);
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
	if (!reader->more) return true;
	return reader->more(reader->data);
}

// Frees the reader.
pub void free(t *reader) {
	if (reader->free) reader->free(reader->data);
	OS.free(reader);
}

bool mem_more(void *ctx) {
	membuf_t *data = ctx;
	return data->pos < data->len;
}

int mem_read(void *ctx, uint8_t *buf, size_t n) {
	membuf_t *s = ctx;
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

int file_read(void *ctx, uint8_t *buf, size_t n) {
	FILE *f = ctx;
	size_t r = fread(buf, 1, n, f);
	if (r == 0) {
		if (feof(f) || ferror(f)) return EOF;
	}
	return (int) r;
}

bool file_more(void *ctx) {
	FILE *f = ctx;
	return !feof(f) && !ferror(f);
}

typedef {
	int fd;
	bool more;
} fd_t;

pub t *fd(int f) {
	t *r = calloc(1, sizeof(t));
	fd_t *state = calloc(1, sizeof(fd_t));
	if (!r || !state) panic("calloc failed");
	state->fd = f;
	r->data = state;
	r->free = OS.free;
	r->more = fd_more;
	r->read = fd_read;
	return r;
}

bool fd_more(void *ctx) {
	fd_t *state = ctx;
	return state->more;
}

int fd_read(void *ctx, uint8_t *buf, size_t n) {
	fd_t *state = ctx;
	int r = OS.read(state->fd, buf, n);
	if (r <= 0) state->more = false;
	return r;
}
