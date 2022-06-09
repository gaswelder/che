pub typedef {
	const char *s;
	size_t pos;
	size_t len;
	size_t col;
	size_t row;
} parsebuf_t;

/*
 * Creates and returns an instance of a parsebuffer with the given string as
 * contents. The string must not be deallocated before the buffer.
 */
pub parsebuf_t *buf_new(const char *s) {
	parsebuf_t *b = calloc(1, sizeof(parsebuf_t));
	if (!b) return NULL;
	b->s = s;
	b->len = strlen(s);
	return b;
}

/*
 * Frees memory used by the buffer.
 */
pub void buf_free(parsebuf_t *b) {
	free(b);
}

/*
 * Returns the next character in the buffer.
 * Returns EOF if there is no next character.
 */
pub int buf_peek(parsebuf_t *b) {
	if (b->pos >= b->len) {
		return EOF;
	}
	return b->s[b->pos];
}

/*
 * Returns true if there is at least one more character in the stream.
 */
pub bool buf_more(parsebuf_t *b) {
	return b->pos < b->len;
}

pub void buf_fcontext(parsebuf_t *b, char *buf, size_t len)
{
	if(len > b->len - b->pos) {
		len = b->len - b->pos;
	}

	size_t i = 0;
	for(i = 0; i < len; i++) {
		buf[i] = b->s[b->pos + i];
	}
	buf[i] = '\0';
}

/*
 * Returns next character in the buffer and removes it from the stream.
 * Returns EOF if there is no next character.
 */
pub int buf_get(parsebuf_t *b)
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

bool haschar(const char *s, char c) {
	const char *p = s;
	while (*p) {
		if (*p == c) return true;
		p++;
	}
	return false;
}

pub void buf_skip_set(parsebuf_t *b, const char *set) {
	while (buf_more(b) && haschar(set, buf_peek(b))) {
		buf_get(b);
	}
}

pub char *buf_read_set(parsebuf_t *b, const char *set) {
	char *s = calloc(10000, 1);
	char *p = s;
	while (buf_more(b) && haschar(set, buf_peek(b))) {
		*p = buf_get(b);
		p++;
	}
	return s;
}

// void ahead(parsebuf_t *b) {
// 	for (size_t i = 0; i < 10; i++) {
// 		putchar(b->s[b->pos + i]);
// 	}
// }

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

/*
 * Returns true if the given literal is next in the buffer.
 */
pub bool buf_literal_follows(parsebuf_t *b, const char *literal) {
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
