#import tokenizer

pub typedef {
	tokenizer.t *b;
	int vals;
	char valbuf[4096];
	bool firstval;

} reader_t;

// Creates a new CSV reader for stdin.
pub reader_t *newreader() {
	reader_t *r = calloc!(1, sizeof(reader_t));
	r->b = tokenizer.stdin();
	r->firstval = true;
	return r;
}

// Releases the reader.
pub void freereader(reader_t *r) {
	if (r->b) tokenizer.free(r->b);
	free(r);
}

// Reads the next column's value in the current line.
// Returns false if there are no more columns in the current line.
pub bool readval(reader_t *r) {
	// If this is not the first column, expect a comma.
	if (!r->firstval) {
		if (tokenizer.peek(r->b) != ',') {
			return false;
		}
		tokenizer.get(r->b);
	}
	r->firstval = false;

	int next = tokenizer.peek(r->b);
	if (next == '\n' || next == '\r') return false;
	if (!tokenizer.more(r->b)) return false;

	if (next == '"') {
		read_quote(r);
		return true;
	}

	char *p = r->valbuf;
	while (tokenizer.more(r->b) && !peekany(r->b, ",\r\n")) {
		*p++ = tokenizer.get(r->b);
	}
	*p = '\0';
	return true;
}

bool peekany(tokenizer.t *b, const char *chars) {
	const char *p = chars;
	while (*p != '\0') {
		if (tokenizer.peek(b) == *p) {
			return true;
		}
		p++;
	}
	return false;
}

pub bool nextline(reader_t *r) {
	bool ok = false;
	while (tokenizer.peek(r->b) == '\r' || tokenizer.peek(r->b) == '\n') {
		ok = true;
		tokenizer.get(r->b);
	}
	r->firstval = true;
	return ok;
}

pub const char *val(reader_t *r) {
	return r->valbuf;
}

void read_quote(reader_t *r) {
	tokenizer.t *b = r->b;

	if (!tokenizer.buf_skip(b, '"')) {
		panic("expected '\"', got '%c'", tokenizer.peek(b));
	}
	char *p = r->valbuf;
	while (tokenizer.more(b) && tokenizer.peek(b) != '\"') {
		*p++ = tokenizer.get(b);
	}
	*p = '\0';
	if (!tokenizer.buf_skip(b, '"')) {
		panic("expected '\"', got %c", tokenizer.peek(b));
	}
}

pub typedef {
	FILE *f;
	int col;
} writer_t;

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
