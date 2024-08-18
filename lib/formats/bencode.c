
pub typedef {
	const char *data;
	size_t size;
	size_t pos;

	char keybuf[100];

	int stacksize;
	char stack[10];
} reader_t;

/**
 * Allocates a new reader for the given data.
 */
pub reader_t *newreader(const char *data, size_t size) {
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
	char c = pop(r);
    r->stack[r->stacksize++] = c;
	if (c == 'd' && peek(r) != 'e') {
		loadkey(r);
	}
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
 * Returns current value's key, in case we're inside a dictionary.
 */
pub const char *key(reader_t *r) {
	return r->keybuf;
}

/**
 * Reads the number that follows.
 */
pub int readnum(reader_t *r) {
	consume(r, 'i');
	int n = num(r);
	consume(r, 'e');

	if (more(r) && parent(r) == 'd') {
		loadkey(r);
	}
	return n;
}

/**
 * Reads the string at the current position into the given buffer.
*/
pub bool readstr(reader_t *r, char *buf, size_t bufsize) {
	// <length>:
	int n = num(r);
	if (n < 0) panic("negative string length");
	size_t nz = (size_t) n;
	consume(r, ':');

	// If no buffer is given, discard the data.
	if (!buf) {
		for (size_t i = 0; i < nz; i++) {
			pop(r);
		}
		if (more(r) && parent(r) == 'd') {
			loadkey(r);
		}
		return true;
	}

	// If a buffer is given, make sure it's big enough
	// and copy data there.
	if (nz >= bufsize) {
		panic("buf too small");
	}
    for (size_t i = 0; i < nz; i++) {
		buf[i] = pop(r);
    }
	if (more(r) && parent(r) == 'd') {
		loadkey(r);
	}
	return true;
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

pub void skip(reader_t *r) {
    switch (peek(r)) {
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
	if (more(r) && parent(r) == 'd') {
		loadkey(r);
	}
}

int parent(reader_t *r) {
	return r->stack[r->stacksize-1];
}

void consume(reader_t *r, char c) {
    if (r->data[r->pos] != c) {
        panic("wanted %c, got %c", c, r->data[r->pos]);
    }
    r->pos++;
}

void loadkey(reader_t *r) {
	int len = num(r);
	consume(r, ':');
	for (int i = 0; i < len; i++) {
		r->keybuf[i] = pop(r);
	}
	r->keybuf[len] = 0;
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


/*
#import formats/bencode
#import fs

int main() {
    size_t size = 0;
    char *data = fs.readfile("a.torrent", &size);
    if (!data) {
        fprintf(stderr, "failed to read file: %s\n", strerror(errno));
        return 1;
    }

    bencode.reader_t *r = bencode.newreader(data, size);
    bencode.enter(r);
    while (bencode.more(r)) {
        printf("[%s] = %c\n", bencode.key(r), bencode.type(r));
        bencode.skip(r);
    }
    bencode.leave(r);
    bencode.freereader(r);
    return 0;
}
*/
