#import parsebuf
#import tok.c

pub tok.tok_t **readall(parsebuf.parsebuf_t *b) {
	tok.tok_t **all = calloc(100, sizeof(b));
	size_t n = 0;
	while (true) {
		tok.tok_t *t = read(b);
		if (!t) break;
		all[n++] = t;
	}
	return all;
}

// Reads next item from the buffer.
pub tok.tok_t *read(parsebuf.parsebuf_t *b) {
	parsebuf.spaces(b);
	if (!parsebuf.buf_more(b)) {
		return NULL;
	}
	if (parsebuf.peek(b) == '(') {
		return readlist(b);
	}
	if (isdigit(parsebuf.peek(b))) {
		return readnum(b);
	}
	return readsymbol(b);
}

// Reads a number.
tok.tok_t *readnum(parsebuf.parsebuf_t *b) {
	char buf[100];
	if (!parsebuf.num(b, buf, 100)) {
		panic("failed to read a number");
	}
	return tok.newnumber(buf);
}

// Reads a symbol.
tok.tok_t *readsymbol(parsebuf.parsebuf_t *b) {
	tok.tok_t *t = tok.newsym("");
	int pos = 0;
	while (parsebuf.buf_more(b) && !isspace(parsebuf.peek(b)) && parsebuf.peek(b) != ')') {
		t->name[pos++] = parsebuf.buf_get(b);
	}
	return t;
}


// Reads a list.
tok.tok_t *readlist(parsebuf.parsebuf_t *b) {
	tok.tok_t *t = tok.newlist();

	parsebuf.buf_get(b); // "("
	parsebuf.spaces(b);
	while (parsebuf.peek(b) != EOF && parsebuf.peek(b) != ')') {
		t->items[t->nitems++] = read(b);
		parsebuf.spaces(b);
	}
	if (parsebuf.peek(b) != ')') {
		panic("expected )");
	}
	parsebuf.buf_get(b); // ")"
	return t;
}
