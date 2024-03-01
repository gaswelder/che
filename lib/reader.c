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
};

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
	OS.free(reader->data);
	OS.free(reader);
}

pub int peek(t *reader) {
	switch (reader->type) {
		case STR: {
			str_t *data = reader->data;
			if (data->pos >= data->len) return EOF;
			return data->s[data->pos];
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
	}
	panic("unknown type: %d", reader->type);
}
