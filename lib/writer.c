
pub typedef {
	char *data;
	size_t pos;
	size_t size;
} t;

// Returns a writer to a static buffer.
// When the buffer's place runs out, write will return EOF.
pub t *static_buffer(char *data, size_t size) {
	t *w = calloc(1, sizeof(t));
	if (!w) panic("calloc failed");
	w->data = data;
	w->size = size;
	return w;
}

pub void free(t *w) {
	OS.free(w);
}

// Writes n bytes from data to w.
// Returns the number of bytes written.
pub int write(t *w, const char *data, size_t n) {
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
