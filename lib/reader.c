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

pub t *stdin() {
	t *r = calloc(1, sizeof(t));
	if (!r) return NULL;
	r->type = STDIN;
	return r;
}

pub t *string(const char *s) {
	t *r = calloc(1, sizeof(t));
	if (!r) {
		return NULL;
	}
	r->type = STR;
	str_t *data = calloc(1, sizeof(str_t));
	if (!data) {
		OS.free(r);
		return NULL;
	}
	data->s = s;
	data->len = strlen(s);
	r->data = data;
	return r;
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
