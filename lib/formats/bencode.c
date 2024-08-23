
pub typedef {
	/*
	 * The input.
	 */
	const uint8_t *data;
	size_t size;
	size_t pos;

	/*
	 * Stack for tracking the type of node we're currently in.
	 */
	size_t stacksize;
	char stack[10];
} reader_t;

/**
 * Allocates a new reader for the given data.
 */
pub reader_t *newreader(const uint8_t *data, size_t size) {
	reader_t *r = calloc(1, sizeof(reader_t));
	if (!r) return NULL;
	r->data = data;
	r->size = size;
	return r;
}

/**
 * Frees the reader.
 */
pub void freereader(reader_t *r) {
	free(r);
}

/**
 * Returns true if the current node has more entries.
 */
pub bool more(reader_t *r) {
    if (r->stacksize > 0) {
        return peek(r) != 'e';
    }
    return peek(r) != EOF;
}

/**
 * Returns the type of the value that follows.
 * Returns 0 at the end of file.
 */
pub int type(reader_t *r) {
	switch (peek(r)) {
		case EOF: { return 0; }
		case 'i': { return 'i'; }
		case 'd': { return 'd'; }
		case 'l': { return 'l'; }
		default: { return 's'; }
	}
}

/**
 * Enters the current node.
 * The current node must be a dictionary or a list.
 */
pub void enter(reader_t *r) {
	if (r->stacksize == sizeof(r->stack)) {
		panic("stack too small");
	}
    r->stack[r->stacksize++] = pop(r);
}

/**
 * Leaves the current node.
 */
pub void leave(reader_t *r) {
	// Skip unread contents
	// while (more(r)) skip(r);

    consume(r, 'e');
    r->stacksize--;
}

/**
 * When inside a dictionary, reads the next entry's key into buf.
 * Returns the key length in bytes.
 * Keep in mind that keys are byte buffers, not guaranteed to be ASCIIZ.
 */
pub size_t key(reader_t *r, uint8_t *buf, size_t len) {
	return _readbuf(r, buf, len);
}

/**
 * Returns the following string's length.
 */
pub int strsize(reader_t *r) {
	size_t p = r->pos;
	int n = num(r);
	r->pos = p;
	return n;
}

/**
 * Reads the string at the current position into the given buffer.
*/
pub size_t readbuf(reader_t *r, uint8_t *buf, size_t bufsize) {
	return _readbuf(r, buf, bufsize);
}

size_t _readbuf(reader_t *r, uint8_t *buf, size_t len) {
	int n = num(r);
	if (n < 0) panic("got negative data length");
	size_t nz = (size_t) n;
	if (buf != NULL && nz > len) panic("buffer too small (need %d)", n);

	consume(r, ':');

	// Write the data to the buffer.
	if (buf) {
		memset(buf, 0, len);
		for (int i = 0; i < n; i++) {
			buf[i] = pop(r);
		}
	} else {
		for (int i = 0; i < n; i++) {
			pop(r);
		}
	}
	return nz;
}

/**
 * Reads the number that follows.
 */
pub int readnum(reader_t *r) {
	consume(r, 'i');
	int n = num(r);
	consume(r, 'e');
	return n;
}

/**
 * Skips to the next entry.
 * Returns false if there is no next entry.
 */
pub bool skip(reader_t *r) {
    switch (peek(r)) {
		case EOF: {
			return false;
		}
        case 'i': {
			consume(r, 'i');
			num(r);
			consume(r, 'e');
		}
        case 'l', 'd': {
            enter(r);
            while (more(r)) skip(r);
            leave(r);
        }
        default: {
			// string
			int n = num(r);
			consume(r, ':');
			for (int i = 0; i < n; i++) {
				pop(r);
			}
        }
    }
	return true;
}

// int parent(reader_t *r) {
// 	return r->stack[r->stacksize-1];
// }

void consume(reader_t *r, char c) {
    if (r->data[r->pos] != c) {
        panic("wanted %c, got %c", c, r->data[r->pos]);
    }
    r->pos++;
}


char peek(reader_t *r) {
    if (r->pos >= r->size) {
        return EOF;
    }
    return r->data[r->pos];
}

int num(reader_t *r) {
    int n = 0;
    bool neg = false;
    if (peek(r) == '-') {
        neg = true;
        pop(r);
    }
    if (!isdigit(peek(r))) {
        panic("expected a digit, got %c (%x)", peek(r), peek(r));
    }
    while (isdigit(peek(r))) {
        n *= 10;
        n += pop(r) - '0';
    }
    if (neg) n *= -1;
    return n;
}

char pop(reader_t *r) {
    return r->data[r->pos++];
}
