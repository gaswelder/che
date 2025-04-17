#import tokenizer
#import tok.c

pub tok.tok_t **readall(tokenizer.t *b) {
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
pub tok.tok_t *read(tokenizer.t *b) {
	tokenizer.spaces(b);
	if (!tokenizer.more(b)) {
		return NULL;
	}
	if (tokenizer.peek(b) == '(') {
		return readlist(b);
	}
	if (isdigit(tokenizer.peek(b))) {
		return readnum(b);
	}
	return readsymbol(b);
}

// Reads a number.
tok.tok_t *readnum(tokenizer.t *b) {
	char buf[100];
	if (!tokenizer.num(b, buf, 100)) {
		panic("failed to read a number");
	}
	return tok.newnumber(buf);
}

// Reads a symbol.
tok.tok_t *readsymbol(tokenizer.t *b) {
	tok.tok_t *t = tok.newsym("");
	int pos = 0;
	while (tokenizer.more(b) && !isspace(tokenizer.peek(b)) && tokenizer.peek(b) != ')') {
		t->name[pos++] = tokenizer.get(b);
	}
	return t;
}


// Reads a list.
tok.tok_t *readlist(tokenizer.t *b) {
	tok.tok_t *t = tok.newlist();

	tokenizer.get(b); // "("
	tokenizer.spaces(b);
	while (tokenizer.peek(b) != EOF && tokenizer.peek(b) != ')') {
		t->items[t->nitems++] = read(b);
		tokenizer.spaces(b);
	}
	if (tokenizer.peek(b) != ')') {
		panic("expected )");
	}
	tokenizer.get(b); // ")"
	return t;
}
