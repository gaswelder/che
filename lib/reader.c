pub typedef {
	const char *s;
	size_t pos;
	size_t len;
} t;

pub t *string(const char *s) {
	t *r = calloc(1, sizeof(t));
	if (!r) {
		return NULL;
	}
	r->s = s;
	r->len = strlen(s);
	return r;
}

pub void free(t *reader) {
	OS.free(reader);
}

pub int peek(t *b) {
	if (b->pos >= b->len) {
		return EOF;
	}
	return b->s[b->pos];
}

pub bool more(t *b) {
	return b->pos < b->len;
}

pub int get(t *b) {
	if (b->pos >= b->len) {
		return EOF;
	}
	return b->s[b->pos++];
}
