#import rt.c

// Reads next item from stdin.
pub rt.item_t *read() {
	skipspaces();
	if (peek() == '(') {
		return readlist();
	}
	return readsymbol();
}

// Reads a symbol.
rt.item_t *readsymbol() {
	char buf[60] = {0};
	int pos = 0;
	while (peek() != EOF && !isspace(peek()) && peek() != ')') {
		buf[pos++] = getchar();
	}
	return rt.sym(rt.intern(buf));
}

// Reads a list.
rt.item_t *readlist() {
	getchar(); // "("
	rt.item_t *r = NULL;

	skipspaces();
	while (peek() != EOF && peek() != ')') {
		r = rt.cons(read(), r);
		skipspaces();
	}
	if (peek() != ')') {
		panic("expected )");
	}
	getchar(); // ")"
	return rt.reverse(r);
}

void skipspaces() {
	while (isspace(peek())) {
		getchar();
	}
}

int peek() {
	int c = getchar();
	if (c == EOF) {
		return EOF;
	}
	ungetc(c, stdin);
	return c;
}
