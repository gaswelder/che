#import reader

pub typedef {
	reader.t *reader; // Byte reader.
	bool reader_own;

	size_t row, col; // Position in the source text.

	// Lookahead cache.
	char cache[100];
	size_t cachesize;
} parsebuf_t;

pub parsebuf_t *from_stdin() {
	parsebuf_t *b = new(reader.stdin());
	b->reader_own = true;
	return b;
}

// Returns a new parsebuf instance reading from the given string.
// The string must not be deallocated before the buffer.
pub parsebuf_t *buf_new(const char *s) {
	parsebuf_t *b = new(reader.string(s));
	b->reader_own = true;
	return b;
}

// Returns a parsebuf instance reading from the given reader.
pub parsebuf_t *new(reader.t *r) {
	if (!r) panic("got null reader");
	parsebuf_t *b = calloc(1, sizeof(parsebuf_t));
	if (!b) panic("calloc failed");
	b->reader = r;
	return b;
}

// Frees memory used by the buffer.
pub void buf_free(parsebuf_t *b) {
	if (b->reader_own) {
		reader.free(b->reader);
	}
	free(b);
}

/*
 * Returns the next character in the buffer.
 * Returns EOF if there is no next character.
 */
pub int buf_peek(parsebuf_t *b) {
	if (b->cachesize > 0) {
		return b->cache[0];
	}
	return reader.peek(b->reader);
}

/*
 * Returns true if there is at least one more character in the stream.
 */
pub bool buf_more(parsebuf_t *b) {
	return b->cachesize > 0 || reader.more(b->reader);
}

pub void buf_fcontext(parsebuf_t *b, char *buf, size_t len) {
	_prefetch(b, len);

	size_t n = len-1;
	if (b->cachesize < n) {
		n = b->cachesize;
	}
	for (size_t i = 0; i < n; i++) {
		buf[i] = b->cache[i];
	}
	buf[len-1] = '\0';
}

/*
 * Returns next character in the buffer and removes it from the stream.
 * Returns EOF if there is no next character.
 */
pub int buf_get(parsebuf_t *b) {
	if (b->cachesize > 0) {
		int c = b->cache[0];
		for (size_t i = 0; i < b->cachesize-1; i++) {
			b->cache[i] = b->cache[i+1];
		}
		b->cachesize--;
		_track_pos(b, c);
		return c;
	}

	int c = reader.get(b->reader);
	_track_pos(b, c);
	return c;
}

void _track_pos(parsebuf_t *b, int c) {
	if (c == '\n') {
		b->col = 0;
		b->row++;
	} else {
		b->col++;
	}
}

void _prefetch(parsebuf_t *b, size_t n) {
	while (b->cachesize < n && reader.more(b->reader)) {
		b->cache[b->cachesize++] = reader.get(b->reader);
	}
}

pub void buf_skip_set(parsebuf_t *b, const char *set) {
	while (buf_more(b) && strchr(set, buf_peek(b))) {
		buf_get(b);
	}
}

/*
 * Skips one character if it's equal to c.
 */
pub bool buf_skip(parsebuf_t *b, char c) {
	if (buf_peek(b) == c) {
		buf_get(b);
		return true;
	}
	return false;
}

pub char *buf_read_set(parsebuf_t *b, const char *set) {
	char *s = calloc(10000, 1);
	char *p = s;
	while (buf_more(b) && strchr(set, buf_peek(b))) {
		*p = buf_get(b);
		p++;
	}
	return s;
}

pub bool buf_skip_literal(parsebuf_t *b, const char *literal) {
	if (!buf_literal_follows(b, literal)) {
		return false;
	}
	size_t n = strlen(literal);
	for (size_t i = 0; i < n; i++) {
		buf_get(b);
	}
	return true;
}

pub bool spaces(parsebuf_t *b) {
	int n = 0;
	while (isspace(buf_peek(b))) {
		n++;
		buf_get(b);
	}
	return n > 0;
}

/**
 * Reads a C-style identifier into the provided buffer.
 * If the buffer is too small, the identifier is still read
 * completely, but trimmed to fit the buffer.
 */
pub bool id(parsebuf_t *b, char *buf, size_t n) {
	if (!isalpha(buf_peek(b)) && buf_peek(b) != '_') {
		return false;
	}
	size_t pos = 0;
	while (buf_more(b)) {
		int c = buf_peek(b);
		if (!isalpha(c) && !isdigit(c) && c != '_') {
			break;
		}
		buf_get(b);
		if (pos < n-1) {
			buf[pos++] = c;
		}
	}
	buf[pos++] = '\0';
	return true;
}

pub bool num(parsebuf_t *b, char *buf, size_t n) {
	(void) n;
	char *p = buf;

	if (buf_peek(b) == '-') {
		*p++ = buf_get(b);
	}

	// [digits]
	if (!isdigit(buf_peek(b))) {
		return false;
	}
	while (isdigit(buf_peek(b))) {
		*p++ = buf_get(b);
	}

	// Optional fractional part
	if (buf_peek(b) == '.') {
		*p++ = buf_get(b);
		if (!isdigit(buf_peek(b))) {
			return false;
		}
		while (isdigit(buf_peek(b))) {
			*p++ = buf_get(b);
		}
	}

	// Optional exponent
	int c = buf_peek(b);
	if (c == 'e' || c == 'E') {
		*p++ = buf_get(b);
		
		// Optional - or +
		c = buf_peek(b);
		if (c == '-' || c == '+') {
			*p++ = buf_get(b);
		}

		// Sequence of exponent digits
		if (!isdigit(buf_peek(b))) {
			return false;
		}
		while (isdigit(buf_peek(b))) {
			*p++ = buf_get(b);
		}
	}
	return true;
}

/*
 * Returns true if the given literal is next in the buffer.
 */
pub bool buf_literal_follows(parsebuf_t *b, const char *literal) {
	_prefetch(b, strlen(literal));
	if (b->cachesize < strlen(literal)) {
		return false;
	}

	const char *p1 = b->cache;
	const char *p2 = literal;
	while (*p2) {
		if (!*p1 || *p1 != *p2) {
			return false;
		}
		p1++;
		p2++;
	}
	return true;
}

pub char *buf_pos(parsebuf_t *b) {
	char *s = calloc(10, 1);
	sprintf(s, "%zu:%zu", b->row + 1, b->col + 1);
	return s;
}

pub char *buf_skip_until(parsebuf_t *b, const char *literal) {
	char *s = calloc(10000, 1);
	char *p = s;

	while (buf_more(b)) {
		if (buf_literal_follows(b, literal)) {
			break;
		}
		*p = buf_get(b);
		p++;
	}
	return s;
}
