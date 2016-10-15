struct __parsebuf {
	const char *s;
	size_t pos;
	size_t len;
};

typedef struct __parsebuf parsebuf;

/*
 * Creates and returns an instance of a parsebuffer with
 * the given string as the contents. The string must not be
 * deallocated before the buffer is.
 */
parsebuf *buf_new(const char *s)
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
void buf_free(parsebuf *b)
{
	free(b);
}

/*
 * Returns the next character in the buffer.
 * Returns EOF if there is no next character.
 */
int buf_peek(parsebuf *b)
{
	if(b->pos >= b->len) {
		return EOF;
	}
	return b->s[b->pos];
}

void buf_fcontext(parsebuf *b, char *buf, size_t len)
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
int buf_get(parsebuf *b)
{
	if(b->pos >= b->len) {
		return EOF;
	}
	int c = b->s[b->pos];
	b->pos++;
	return c;
}

/*
 * Returns true if there is at least one more character
 * in the stream.
 */
bool buf_more(parsebuf *b)
{
	return b->pos < b->len;
}
