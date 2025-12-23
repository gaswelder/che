#include <unistd.h>

pub typedef int writefunc_t(void *, const uint8_t *, size_t); // ctx, data, len
pub typedef void freefunc_t(void *);

pub typedef {
	size_t nwritten; // How many bytes have been written.
	void *data;
	writefunc_t *write;
	freefunc_t *free;
} t;

// Writes n bytes from data to w.
// Returns the number of bytes written or -1 on error.
pub int write(t *w, const uint8_t *data, size_t n) {
	int r = w->write(w->data, data, n);
	if (r > 0) w->nwritten += r;
	return r;
}

// Frees memory used by the writer w.
pub void free(t *w) {
	if (w->free) {
		w->free(w->data);
	}
	OS.free(w);
}

pub int writebyte(t *w, uint8_t b) {
	return write(w, &b, 1);
}

pub t *new(void *data, writefunc_t *write, freefunc_t *free) {
	t *w = calloc!(1, sizeof(t));
	w->data = data;
	w->write = write;
	w->free = free;
	return w;
}

//
// file writer
//

// Allocates and returns a writer to file f.
pub t *file(FILE *f) {
	return new(f, file_write, NULL);
}

int file_write(void *ctx, const uint8_t *data, size_t n) {
	FILE *f = ctx;
	size_t r = fwrite(data, 1, n, f);
	return (int) r;
}

t *_stdout = NULL;

// Returns a global instance of an stdout writer.
// This function is not thread safe.
pub t *stdout() {
	if (!_stdout) {
		_stdout = file(OS.stdout);
	}
	return _stdout;
}


//
// memory writer
//

typedef { uint8_t *data; size_t pos, size; } membuf_t;

// Returns a writer to a static buffer.
// When the buffer's space runs out, write will return EOF.
pub t *static_buffer(uint8_t *data, size_t size) {
	membuf_t *s = calloc!(1, sizeof(membuf_t));
	s->data = data;
	s->size = size;
	return new(s, mem_write, OS.free);
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


//
// fd writer
//

typedef { int fd; } fd_t;

// Returns a writer writing to the file descriptor f.
pub t *fd(int f) {
	fd_t *state = calloc!(1, sizeof(fd_t));
	state->fd = f;
	return new(state, fd_write, OS.free);
}

int fd_write(void *ctx, const uint8_t *data, size_t n) {
	fd_t *state = ctx;
	return OS.write(state->fd, data, n);
}
