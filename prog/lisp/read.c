#import rt.c
#import parsebuf

// Reads next item from the buffer.
pub rt.item_t *read(parsebuf.parsebuf_t *b) {
	parsebuf.spaces(b);
	if (parsebuf.peek(b) == '(') {
		return readlist(b);
	}
	return readsymbol(b);
}

// Reads a symbol.
rt.item_t *readsymbol(parsebuf.parsebuf_t *b) {
	char buf[60] = {0};
	int pos = 0;
	while (parsebuf.buf_more(b) && !isspace(parsebuf.peek(b)) && parsebuf.peek(b) != ')') {
		buf[pos++] = parsebuf.buf_get(b);
	}
	return rt.sym(rt.intern(buf));
}

// Reads a list.
rt.item_t *readlist(parsebuf.parsebuf_t *b) {
	parsebuf.buf_get(b); // "("
	rt.item_t *r = NULL;

	parsebuf.spaces(b);
	while (parsebuf.peek(b) != EOF && parsebuf.peek(b) != ')') {
		r = rt.cons(read(b), r);
		parsebuf.spaces(b);
	}
	if (parsebuf.peek(b) != ')') {
		panic("expected )");
	}
	parsebuf.buf_get(b); // ")"
	return rt.reverse(r);
}
