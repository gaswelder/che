struct __parsebuf {
	const char *s;
	size_t pos;
	size_t len;
	size_t col;
	size_t row;
};

typedef struct __parsebuf parsebuf;

/*
 * Creates and returns an instance of a parsebuffer with
 * the given string as the contents. The string must not be
 * deallocated before the buffer is.
 */
pub parsebuf *buf_new(const char *s)
{
	parsebuf *b = calloc(1, sizeof(parsebuf));
	if(!b) return NULL;
	b->s = s;
	b->len = strlen(s);
	return b;
}

/*
 * Frees the memory used by the buffer.
 */
pub void buf_free(parsebuf *b)
{
	free(b);
}

/*
 * Returns the next character in the buffer.
 * Returns EOF if there is no next character.
 */
pub int buf_peek(parsebuf *b)
{
	if (b->pos >= b->len) {
		return EOF;
	}
	return b->s[b->pos];
}

pub void buf_fcontext(parsebuf *b, char *buf, size_t len)
{
	if(len > b->len - b->pos) {
		len = b->len - b->pos;
	}

	size_t i;
	for(i = 0; i < len; i++) {
		buf[i] = b->s[b->pos + i];
	}
	buf[i] = '\0';
}

/*
 * Returns the next character in the buffer and removes
 * it from the stream. Returns EOF if there is no next character.
 */
pub int buf_get(parsebuf *b)
{
	if (b->pos >= b->len) {
		return EOF;
	}
	int c = b->s[b->pos];
	if (c == '\n') {
		b->col = 0;
		b->row++;
	} else {
		b->col++;
	}
	b->pos++;
	return c;
}

/*
 * Returns true if there is at least one more character
 * in the stream.
 */
pub bool buf_more(parsebuf *b)
{
	return b->pos < b->len;
}

bool haschar(const char *s, char c) {
	const char *p = s;
	while (*p) {
		if (*p == c) return true;
		p++;
	}
	return false;
}

pub void buf_skip_set(parsebuf *b, const char *set) {
	while (buf_more(b) && haschar(set, buf_peek(b))) {
		buf_get(b);
	}
}

pub char *buf_read_set(parsebuf *b, const char *set) {
	char *s = calloc(10000, 1);
	char *p = s;
	while (buf_more(b) && haschar(set, buf_peek(b))) {
		*p = buf_get(b);
		p++;
	}
	return s;
}

// void ahead(parsebuf *b) {
// 	for (size_t i = 0; i < 10; i++) {
// 		putchar(b->s[b->pos + i]);
// 	}
// }

pub bool buf_skip_literal(parsebuf *b, const char *literal) {
	if (!buf_literal_follows(b, literal)) {
		return false;
	}
	size_t n = strlen(literal);
	for (size_t i = 0; i < n; i++) {
		buf_get(b);
	}
	return true;
}

/*
 * Returns true if the given literal is next in the buffer.
 */
pub bool buf_literal_follows(parsebuf *b, const char *literal) {
	const char *p1 = b->s + b->pos;
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

pub char *buf_pos(parsebuf *b) {
	char *s = calloc(10, 1);
	sprintf(s, "%zu:%zu", b->row + 1, b->col + 1);
	return s;
}

pub char *buf_skip_until(parsebuf *b, const char *literal) {
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
