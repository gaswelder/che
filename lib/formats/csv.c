#import tokenizer

pub typedef {
	tokenizer.t *b;
	int vals;
	char line[4096];
} reader_t;

pub typedef {
	FILE *f;
	int col;
} writer_t;

pub reader_t *new_reader() {
	reader_t *r = calloc(1, sizeof(reader_t));
	return r;
}

/**
 * Loads next line for parsing.
 * Returns false on error or end of file; check errno on false.
 */
pub bool read_line(reader_t *r) {
	if (!fgets(r->line, sizeof(r->line), stdin)) return false;
	if (r->b) {
		tokenizer.free(r->b);
		r->b = NULL;
	}
	r->b = tokenizer.from_str(r->line);
	r->vals = 0;
	return true;
}

pub void free_reader(reader_t *r) {
	if (r->b) tokenizer.free(r->b);
	free(r);
}

/**
 * Reads the next value from the current line into buf.
 * Returns false if there are no more values.
 */
pub bool read_val(reader_t *r, char *buf, size_t bufsize) {
	int next = tokenizer.peek(r->b);
	if (next == '\n' || next == '\r') return false;
	if (!tokenizer.more(r->b)) return false;

	char *p = buf;
	size_t n = 0;
	tokenizer.t *b = r->b;

	if (r->vals > 0) {
		if (!tokenizer.buf_skip(b, ',')) panic("expected ',' (vals=%d, next=%c)", r->vals, next);
	}

	if (!tokenizer.buf_skip(b, '"')) panic("expected '\"'");
	while (tokenizer.more(b) && tokenizer.peek(b) != '\"') {
		if (n + 1 == bufsize) panic("buf too small");
		n++;
		*p++ = tokenizer.get(b);
	}
	if (!tokenizer.buf_skip(b, '"')) panic("expected '\"'");
	*p++ = '\0';
	r->vals++;
	return true;
}

// Writes the next column's value.
// If x is null, ends the current line and starts a new one.
pub void writeval(writer_t *w, const char *x) {
	if (x == NULL) {
		w->col = 0;
		fputc('\n', w->f);
		return;
	}
	if (w->col > 0) {
		fputc(',', w->f);
	}
	w->col++;
	fputc('"', w->f);
	while (*x != '\0') {
		if (*x == '"') fputc('\\', w->f);
		fputc(*x, w->f);
		x++;
	}
	fputc('"', w->f);
}
