#import reader

pub typedef {
	reader.t *reader; // Byte reader.
	bool reader_own;

	size_t row, col; // Position in the source text.

	// Lookahead cache.
	uint8_t cache[100];
	size_t cachesize;

	// Buffer for a string with current position.
	char posstrbuf[10];
} t;

// Returns a new instance reading from stdin.
pub t *stdin() {
	t *b = new(reader.stdin());
	b->reader_own = true;
	return b;
}

// Returns a new instance reading from the file f.
pub t *file(FILE *f) {
	t *b = new(reader.file(f));
	b->reader_own = true;
	return b;
}

// Returns a new tokenizer reading from string s.
// The string must not be deallocated while the tokenizer is in use.
pub t *from_str(const char *s) {
	t *b = new(reader.string(s));
	b->reader_own = true;
	return b;
}

// Returns a tokenizer instance reading from the given reader.
pub t *new(reader.t *r) {
	if (!r) panic("got null reader");
	t *b = calloc!(1, sizeof(t));
	b->reader = r;
	return b;
}

// Frees memory used by the buffer.
pub void free(t *b) {
	if (b->reader_own) {
		reader.free(b->reader);
	}
	OS.free(b);
}

// Returns the next character in the buffer.
// Returns EOF if there is no next character.
pub int peek(t *b) {
	_prefetch(b, 1);
	if (b->cachesize == 0) return EOF;
	return b->cache[0];
}

// Returns true if there is at least one more character to be read.
pub bool more(t *b) {
	return b->cachesize > 0 || reader.more(b->reader);
}

// Puts up to len following characters into buf without removing them.
pub void tail(t *b, char *buf, size_t len) {
	_prefetch(b, len);

	size_t n = len-1;
	if (n > b->cachesize) {
		n = b->cachesize;
	}
	for (size_t i = 0; i < n; i++) {
		buf[i] = b->cache[i];
	}
	buf[len-1] = '\0';
}

pub void dbg(t *b) {
	char buf[100] = {};
	tail(b, buf, sizeof(buf));
	printf("[%s...\n", buf);
}

// Returns next character in the buffer and removes it from the stream.
// Returns EOF if there is no next character.
pub int get(t *b) {
	if (b->cachesize > 0) {
		int c = b->cache[0];
		for (size_t i = 0; i < b->cachesize-1; i++) {
			b->cache[i] = b->cache[i+1];
		}
		b->cachesize--;
		_track_pos(b, c);
		return c;
	}

	uint8_t c;
	int r = reader.read(b->reader, &c, 1);
	if (r != 1) return EOF;
	_track_pos(b, c);
	return c;
}

void _track_pos(t *b, int c) {
	if (c == '\n') {
		b->col = 0;
		b->row++;
	} else {
		b->col++;
	}
}

void _prefetch(t *b, size_t n) {
	if (b->cachesize >= n) return;
	size_t len = n - b->cachesize;
	int r = reader.read(b->reader, b->cache + b->cachesize, len);
	if (r > 0) b->cachesize += r;
}

pub void buf_skip_set(t *b, const char *set) {
	while (more(b) && strchr(set, peek(b)) != NULL) {
		get(b);
	}
}

/*
 * Skips one character if it's equal to c.
 */
pub bool buf_skip(t *b, char c) {
	if (peek(b) == c) {
		get(b);
		return true;
	}
	return false;
}

pub char *buf_read_set(t *b, const char *set) {
	char *s = calloc!(10000, 1);
	char *p = s;
	while (more(b) && strchr(set, peek(b)) != NULL) {
		*p = get(b);
		p++;
	}
	return s;
}

// Skips ASCII spaces.
// Returns true if at least one character was skipped.
pub bool spaces(t *b) {
	int n = 0;
	while (isspace(peek(b))) {
		n++;
		get(b);
	}
	return n > 0;
}

// Skips space and tab characters.
pub void hspaces(t *b) {
	while (peek(b) == ' ' || peek(b) == '\t') {
		get(b);
	}
}

/**
 * Reads a C-style identifier into the provided buffer.
 * If the buffer is too small, the identifier is still read
 * completely, but trimmed to fit the buffer.
 */
pub bool id(t *b, char *buf, size_t n) {
	if (!isalpha(peek(b)) && peek(b) != '_') {
		return false;
	}
	size_t pos = 0;
	while (more(b)) {
		int c = peek(b);
		if (!isalpha(c) && !isdigit(c) && c != '_') {
			break;
		}
		get(b);
		if (pos < n-1) {
			buf[pos++] = c;
		}
	}
	buf[pos++] = '\0';
	return true;
}

// Reads a C-style floating-point number into buf.
// Returns false on parse error.
pub bool num(t *b, char *buf, size_t n) {
	(void) n;
	char *p = buf;

	if (peek(b) == '-') {
		*p++ = get(b);
	}

	// [digits]
	if (!isdigit(peek(b))) {
		return false;
	}
	while (isdigit(peek(b))) {
		*p++ = get(b);
	}

	// Optional fractional part
	if (peek(b) == '.') {
		*p++ = get(b);
		if (!isdigit(peek(b))) {
			return false;
		}
		while (isdigit(peek(b))) {
			*p++ = get(b);
		}
	}

	// Optional exponent
	int c = peek(b);
	if (c == 'e' || c == 'E') {
		*p++ = get(b);
		
		// Optional - or +
		c = peek(b);
		if (c == '-' || c == '+') {
			*p++ = get(b);
		}

		// Sequence of exponent digits
		if (!isdigit(peek(b))) {
			return false;
		}
		while (isdigit(peek(b))) {
			*p++ = get(b);
		}
	}
	*p++ = '\0';
	return true;
}

// Skips a literal string and returns true.
// Returns false if the literal does not follow.
pub bool skip_literal(t *b, const char *literal) {
	if (!literal_follows(b, literal)) {
		return false;
	}
	size_t n = strlen(literal);
	for (size_t i = 0; i < n; i++) {
		get(b);
	}
	return true;
}

// Returns true if the given literal is next in the buffer.
pub bool literal_follows(t *b, const char *literal) {
	_prefetch(b, strlen(literal));
	if (b->cachesize < strlen(literal)) {
		return false;
	}

	const uint8_t *p1 = b->cache;
	const uint8_t *p2 = (void *) literal;
	while (*p2) {
		if (!*p1 || *p1 != *p2) {
			return false;
		}
		p1++;
		p2++;
	}
	return true;
}

// Returns a pointer to an internal string with the current position
// formatted as line:char.
pub const char *posstr(t *b) {
	snprintf(b->posstrbuf, sizeof(b->posstrbuf), "%zu:%zu", b->row + 1, b->col + 1);
	return b->posstrbuf;
}

pub char *buf_skip_until(t *b, const char *literal) {
	char *s = calloc!(10000, 1);
	char *p = s;

	while (more(b)) {
		if (literal_follows(b, literal)) {
			break;
		}
		*p = get(b);
		p++;
	}
	return s;
}

// Reads characters into buf until the character 'until' is encountered
// or the input ends. Returns true on success and false if the buffer is filled
// without reaching the character.
pub bool read_until(t *b, char until, char *buf, size_t len) {
	size_t pos = 0;
	while (more(b) && peek(b) != until) {
		if (pos == len-1) {
			return false;
		}
		buf[pos++] = get(b);
	}
	buf[pos] = '\0';
	return true;
}
