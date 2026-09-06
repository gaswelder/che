#import scanner

pub typedef {
	scanner.t *b;
	int vals;
	char valbuf[4096];
	bool firstval;

} reader_t;

// Creates a new CSV reader for stdin.
pub reader_t *newreader() {
	reader_t *r = calloc!(1, sizeof(reader_t));
	r->b = scanner.file(stdin);
	r->firstval = true;
	return r;
}

// Releases the reader.
pub void freereader(reader_t *r) {
	if (r->b) scanner.free(r->b);
	free(r);
}

// Reads the next column's value in the current line.
// Returns false if there are no more columns in the current line.
pub bool readval(reader_t *r) {
	// If this is not the first column, expect a comma.
	if (!r->firstval) {
		if (scanner.peek(r->b) != ',') {
			return false;
		}
		scanner.get(r->b);
	}
	r->firstval = false;

	int next = scanner.peek(r->b);
	if (next == '\n' || next == '\r') return false;
	if (!scanner.more(r->b)) return false;

	if (next == '"') {
		read_quoted_value(r);
		return true;
	}

	char *p = r->valbuf;
	while (scanner.more(r->b) && !peekany(r->b, ",\r\n")) {
		*p++ = scanner.get(r->b);
	}
	*p = '\0';
	return true;
}

bool peekany(scanner.t *b, const char *chars) {
	const char *p = chars;
	while (*p != '\0') {
		if (scanner.peek(b) == *p) {
			return true;
		}
		p++;
	}
	return false;
}

// Moves the reader to the next line.
pub bool nextline(reader_t *r) {
	// Discard the remaining columns if the caller hasn't consumed them.
	while (true) if (!readval(r)) break;

	bool ok = false;
	while (scanner.peek(r->b) == '\r' || scanner.peek(r->b) == '\n') {
		ok = true;
		scanner.get(r->b);
	}
	r->firstval = true;
	return ok;
}

// Returns a pointer to the current value's content.
pub const char *val(reader_t *r) {
	return r->valbuf;
}

void read_quoted_value(reader_t *r) {
	scanner.t *b = r->b;

	// Opening quote
	if (!scanner.buf_skip(b, '"')) {
		panic("expected '\"', got '%c'", scanner.peek(b));
	}

	// Content + closing quote
	char *p = r->valbuf;
	while (true) {
		int c = scanner.get(b);
		if (c == EOF) {
			panic("unterminated quoted value");
		}

		if (c == '\"') {
			// Two quotes in a row is an escaped quote.
			if (scanner.peek(b) == '\"') {
				scanner.get(b);
			} else {
				*p = '\0';
				break;
			}
		}
		*p++ = c;
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
