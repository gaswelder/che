void *xrealloc (void *p, size_t size) {
  void *np = realloc (p, size);
  if (np == NULL) panic("realloc failed");
  return np;
}

pub typedef {
	FILE *fid;
	char *str;
	char *strp;

	char *buf, *bufp;
	size_t buflen;

	int *readbuf, *readbufp;
	size_t readbuflen;
} t;

/* Read next character in the stream. */
pub int getc(t *r) {
	int c;
	if (r->readbufp > r->readbuf)
		{
			c = *(r->readbufp);
			r->readbufp--;
			return c;
		}
	if (r->str != NULL)
		{
			c = *(r->strp);
			if (c != '\0')
	r->strp++;
			else
	return EOF;
		}
	else
		c = fgetc (r->fid);
	return c;
}

/* Unread a byte. */
pub void putc(t *r, int c) {
	r->readbufp++;
	if (r->readbufp == r->readbuf + r->readbuflen)
		{
			r->readbuflen *= 2;
			r->readbuf = xrealloc (r->readbuf, sizeof (int) * r->readbuflen);
			r->readbufp = r->readbuf + r->readbuflen / 2;
		}
	*(r->readbufp) = c;
}

/* Consume remaining whitespace on line, including linefeed. */
pub void consume_whitespace(t *r) {
	int c = getc(r);
	while (strchr (" \t\r", c) != NULL)
		c = getc (r);
	if (c != '\n')
		putc (r, c);
}

/* Consume remaining characters on line, including linefeed. */
pub void consume_line (t * r) {
	int c = getc (r);
	while (c != '\n' && c != EOF)
		c = getc (r);
	if (c != '\n')
		putc (r, c);
}

/* Append character to buffer. */
pub void buf_append (t *r, char c) {
	if (r->bufp == r->buf + r->buflen) {
		r->buflen *= 2;
		r->buf = xrealloc (r->buf, r->buflen + 1);
		r->bufp = r->buf + r->buflen / 2;
	}
	*(r->bufp) = c;
	*(r->bufp + 1) = '\0';
	r->bufp++;
}

/* Load into buffer until character, ignoring escaped ones. */
pub int buf_read(t *r, char *halt) {
	int c = getc(r);
	int esc = 0;
	if (c == '\\') {
		c = getc (r);
		esc = 1;
	}
	while ((esc || strchr (halt, c) == NULL) && (c != EOF)) {
		buf_append(r, c);
		c = getc (r);
		esc = 0;
		if (c == '\\') {
			c = getc (r);
			esc = 1;
		}
	}
	putc (r, c);
	return !esc;
}
