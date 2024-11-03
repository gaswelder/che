enum {
	T_STATIC,
	T_FILE,
};

pub typedef {
	int type;

	// T_STATIC:
	char *data;
	size_t pos;
	size_t size;

	// T_FILE:
	FILE *f;
} t;

t *_stdout = NULL;

// Returns a global instance of a stdout writer.
// This function is not thread safe.
pub t *stdout() {
	if (!_stdout) {
		_stdout = file(OS.stdout);
	}
	return _stdout;
}

// Returns a writer to a static buffer.
// When the buffer's place runs out, write will return EOF.
pub t *static_buffer(char *data, size_t size) {
	t *w = calloc(1, sizeof(t));
	if (!w) panic("calloc failed");
	w->type = T_STATIC;
	w->data = data;
	w->size = size;
	return w;
}

// Returns a writer to a file.
pub t *file(FILE *f) {
	t *w = calloc(1, sizeof(t));
	if (!w) panic("calloc failed");
	w->type = T_FILE;
	w->f = f;
	return w;
}

pub void free(t *w) {
	OS.free(w);
}

pub int writebyte(t *w, char b) {
	return write(w, &b, 1);
}

// Writes n bytes from data to w.
// Returns the number of bytes written.
pub int write(t *w, const char *data, size_t n) {
	if (w->type == T_STATIC) return st_write(w, data, n);
	size_t r = fwrite(data, 1, n, w->f);
	return (int) r;
}

int st_write(t *w, const char *data, size_t n) {
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
