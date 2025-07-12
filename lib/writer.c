#include <unistd.h>

pub typedef int writefunc_t(void *, const uint8_t *, size_t); // ctx, data, len
pub typedef void freefunc_t(void *);
pub typedef int closefunc_t(void *);

pub typedef {
	size_t nwritten;
	void *data;
	writefunc_t *write;
	freefunc_t *free;
	closefunc_t *close;
} t;

// Writes n bytes from data to w.
// Returns the number of bytes written.
pub int write(t *w, const uint8_t *data, size_t n) {
	int r = w->write(w->data, data, n);
	if (r > 0) w->nwritten += r;
	return r;
}

pub int close(t *w) {
	if (w->close) return w->close(w->data);
	return -1;
}

pub void free(t *w) {
	if (w->free) w->free(w->data);
}

pub int writebyte(t *w, uint8_t b) {
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
	t *w = calloc!(1, sizeof(t));
	w->data = f;
	w->write = file_write;
	return w;
}

int file_write(void *ctx, const uint8_t *data, size_t n) {
	FILE *f = ctx;
	size_t r = fwrite(data, 1, n, f);
	return (int) r;
}


typedef { uint8_t *data; size_t pos, size; } membuf_t;

// Returns a writer to a static buffer.
// When the buffer's place runs out, write will return EOF.
pub t *static_buffer(uint8_t *data, size_t size) {
	t *w = calloc!(1, sizeof(t));
	membuf_t *s = calloc!(1, sizeof(membuf_t));

	s->data = data;
	s->size = size;

	w->write = mem_write;
	w->free = OS.free;
	w->data = s;
	return w;
}

int mem_write(void *ctx, const uint8_t *data, size_t n) {
	membuf_t *w = ctx;
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

typedef {
	int fd;
} fd_t;

pub t *fd(int f) {
	t *w = calloc!(1, sizeof(t));
	fd_t *state = calloc!(1, sizeof(fd_t));

	state->fd = f;
	w->write = fd_write;
	w->free = OS.free;
	w->close = fd_close;
	w->data = state;
	return w;
}

int fd_write(void *ctx, const uint8_t *data, size_t n) {
	fd_t *state = ctx;
	return OS.write(state->fd, data, n);
}

int fd_close(void *ctx) {
	fd_t *state = ctx;
	return OS.close(state->fd);
}
