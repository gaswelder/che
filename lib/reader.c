pub typedef {
	int type;
	void *data;
	size_t pos;
} t;

typedef {
	const char *s;
	size_t pos;
	size_t len;
} str_t;

enum {
	STR = 1,
	STDIN,
};

pub t *static_buffer(const char *data, size_t n) {
	t *r = calloc(1, sizeof(t));
	str_t *s = calloc(1, sizeof(str_t));
	if (!r || !s) panic("calloc failed");

	r->type = STR;
	r->data = s;
	s->s = data;
	s->len = n;
	return r;
}

pub t *stdin() {
	t *r = calloc(1, sizeof(t));
	if (!r) return NULL;
	r->type = STDIN;
	return r;
}

pub t *string(const char *s) {
	return static_buffer(s, strlen(s));
}

pub void free(t *reader) {
	switch (reader->type) {
		case STR: { OS.free(reader->data); }
		case STDIN: {}
		default: { panic("unknown reader type: %d", reader->type); }
	}
}

pub int read(t *reader, char *buf, size_t n) {
	int r;
	switch (reader->type) {
		case STR: { r = st_read(reader->data, buf, n); }
		case STDIN: { r = io_read(buf, n); }
		default: { panic("unknown reader type: %d", reader->type); }
	}
	if (r > 0) reader->pos += r;
	return r;
}

pub bool more(t *reader) {
	switch (reader->type) {
		case STR: { return st_more(reader->data); }
		case STDIN: { return io_more(); }
	}
	panic("unknown type: %d", reader->type);
}

bool st_more(str_t *data) {
	return data->pos < data->len;
}

int st_read(str_t *s, char *buf, size_t n) {
	if (s->pos >= s->len) return EOF;
	int r = 0;
	for (size_t i = 0; i < n; i++) {
		if (s->pos == s->len) break;
		if (buf) buf[i] = s->s[s->pos++];
		r++;
	}
	return r;
}

bool io_more() {
	return !feof(OS.stdin) && !ferror(OS.stdin);
}

int io_read(char *buf, size_t n) {
	size_t r = fread(buf, 1, n, OS.stdin);
	if (r == 0) {
		if (feof(OS.stdin) || ferror(OS.stdin)) return EOF;
	}
	return (int) r;
}
