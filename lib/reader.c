#include <unistd.h>

pub typedef void freefunc_t(void *);
pub typedef int readfunc_t(void *, uint8_t *, size_t);

pub typedef {
	void *data;
	size_t pos;
	freefunc_t *free;
	readfunc_t *read;
} t;

pub t *new(void *data, readfunc_t *read, freefunc_t *free) {
	t *r = calloc(1, sizeof(t));
	r->data = data;
	r->free = free;
	r->read = read;
	return r;
}

// Reads up to n bytes from r to buf.
// Returns the number of bytes read or a negative value for EOF or an error.
// Zero return means no bytes to read at the moment.
pub int read(t *reader, uint8_t *buf, size_t n) {
	int r = reader->read(reader->data, buf, n);
	if (r > 0) reader->pos += r;
	return r;
}

// Frees the reader.
pub void free(t *reader) {
	if (reader->free) reader->free(reader->data);
	OS.free(reader);
}

//
// File
//

int file_read(void *ctx, uint8_t *buf, size_t n) {
	FILE *f = ctx;
	size_t r = fread(buf, 1, n, f);
	if (r == 0) {
		if (feof(f) || ferror(f)) return EOF;
	}
	return (int) r;
}

// Returns a reader for file f.
pub t *file(FILE *f) {
	return new(f, file_read, NULL);
}

pub t *stdin() {
	return file(OS.stdin);
}

//
// Memory
//

typedef {
	const uint8_t *s;
	size_t pos;
	size_t len;
} membuf_t;

int mem_read(void *ctx, uint8_t *buf, size_t n) {
	membuf_t *s = ctx;
	if (s->pos >= s->len) {
		return 0;
	}
	int r = 0;
	for (size_t i = 0; i < n; i++) {
		if (s->pos == s->len) break;
		if (buf) buf[i] = s->s[s->pos];
		s->pos++;
		r++;
	}
	return r;
}

// Returns a reader for a bytes buffer buf of length n.
pub t *static_buffer(const uint8_t *buf, size_t n) {
	membuf_t *s = calloc!(1, sizeof(membuf_t));
	s->s = buf;
	s->len = n;
	return new(s, mem_read, OS.free);
}

// Return a reader for string s.
pub t *string(const char *s) {
	return static_buffer((uint8_t *)s, strlen(s));
}

//
// File descriptor
//

typedef {
	int fd;
} fd_t;

int fd_read(void *ctx, uint8_t *buf, size_t n) {
	fd_t *state = ctx;
	return OS.read(state->fd, buf, n);
}

// Returns a reader for file descriptor f.
pub t *fd(int f) {
	fd_t *state = calloc!(1, sizeof(fd_t));
	state->fd = f;
	return new(state, fd_read, OS.free);
}
