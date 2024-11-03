pub typedef {
	int type;
	void *data;
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
	if (reader->data) {
		OS.free(reader->data);
	}
	OS.free(reader);
}

pub int peek(t *reader) {
	switch (reader->type) {
		case STR: {
			str_t *data = reader->data;
			if (data->pos >= data->len) return EOF;
			return data->s[data->pos];
		}
		case STDIN: {
			int c = fgetc(OS.stdin);
			if (c == EOF) return c;
			ungetc(c, OS.stdin);
			return c;
		}
	}
	panic("unknown type: %d", reader->type);
}

pub bool more(t *reader) {
	switch (reader->type) {
		case STR: {
			str_t *data = reader->data;
			return data->pos < data->len;
		}
		case STDIN: {
			int c = fgetc(OS.stdin);
			if (c == EOF) return false;
			ungetc(c, OS.stdin);
			return true;
		}
	}
	panic("unknown type: %d", reader->type);
}

pub int get(t *reader) {
	switch (reader->type) {
		case STR: {
			str_t *data = reader->data;
			if (data->pos >= data->len) {
				return EOF;
			}
			return data->s[data->pos++];
		}
		case STDIN: {
			return fgetc(OS.stdin);
		}
	}
	panic("unknown type: %d", reader->type);
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

pub int read(t *reader, char *buf, size_t n) {
	if (reader->type == STR) {
		return st_read(reader->data, buf, n);
	}
	int r = 0;
	for (size_t i = 0; i < n; i++) {
		int c = get(reader);
		if (c == EOF && r == 0) {
			return EOF;
		}
		printf("read %zu: got %d\n", i, c);
		if (c < 0) break;
		if (buf) buf[i] = c;
		r++;
	}
	return r;
}
