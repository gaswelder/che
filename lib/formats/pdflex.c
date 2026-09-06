#import tokenizer

pub typedef { char name[10], content[200]; } tok_t;

pub typedef {
	tokenizer.t *tok;
	tok_t peekbuf[3];
	int peeksize;
} lexer_t;

pub lexer_t *new(FILE *f) {
	lexer_t *t = calloc!(1, sizeof(lexer_t));
	t->tok = tokenizer.file(f);
	return t;
}

pub void free(lexer_t *t) {
	tokenizer.free(t->tok);
	OS.free(t);
}

pub void init(lexer_t *t) {
	tokenizer.t *tok = t->tok;
	
	// %PDF-1.2
	if (!tokenizer.skip_literal(tok, "%PDF-1.2\n")) panic("pdf-1.2");

	// %âãÏÓ
	if (!tokenizer.skip_literal(tok, "%")) panic("%%");
	for (int i = 0; i < 4; i++) {
		tokenizer.get(tok);
	}
	if (!tokenizer.skip_literal(tok, "\r\n")) panic("\n");
}

tok_t newtok(const char *name, *content) {
	tok_t t = {};
	strcpy(t.name, name);
	strcpy(t.content, content);
	// printf("+ [%s %s]\n", name, content);
	return t;
}

pub void bytes(lexer_t *t, int n) {
	tokenizer.spaces(t->tok);
	if (t->peeksize > 0) {
		panic("peek cached");
	}
	tokenizer.t *tok = t->tok;
	for (int i = 0; i < n; i++) {
		tokenizer.get(tok);
	}
}

pub tok_t peek(lexer_t *t, int n) {
	if (n > 2) {
		panic("peek buffer has only 3");
	}
	while (t->peeksize < n+1) {
		if (!_read(t, &t->peekbuf[t->peeksize++])) {
			panic("no more");
		}
	}
	return t->peekbuf[n];
}

pub bool more(lexer_t *t) {
	if (t->peeksize > 0) {
		return true;
	}
	if (!_read(t, &t->peekbuf[0])) {
		return false;
	}
	t->peeksize++;
	return true;
}

pub tok_t get(lexer_t *t) {
	if (t->peeksize > 0) {
		tok_t r = t->peekbuf[0];
		for (int i = 1; i < t->peeksize; i++) {
			t->peekbuf[i-1] = t->peekbuf[i];
		}
		t->peeksize--;
		return r;
	}
	tok_t r = {};
	if (!_read(t, &r)) {
		panic("no more");
	}
	return r;
}

const char *delims[] = {
	"<<", ">>", "[", "]",
	"R", "obj", "endobj",
	"xref", "trailer", "startxref",
	"%%EOF", "stream", "endstream",
	"false",
	"n", "f",
};

bool _read(lexer_t *t, tok_t *r) {
	tokenizer.t *tok = t->tok;
	tokenizer.spaces(tok);
	if (!tokenizer.more(tok)) {
		return false;
	}

	for (int i = 0; i < (int) nelem(delims); i++) {
		if (tokenizer.skip_literal(tok, delims[i])) {
			*r = newtok(delims[i], "");
			return true;
		}
	}
	if (tokenizer.skip_literal(tok, "(")) {
		char buf[100] = {};
		int i = 0;
		while (true) {
			if (tokenizer.peek(tok) == ')') {
				break;
			}
			int c = tokenizer.get(tok);
			if (c == '\\') {
				c = tokenizer.get(tok);
			}
			buf[i++] = c;
			if (i+1 == sizeof(buf)) {
				panic("buf too small for a string");
			}
		}
		if (!tokenizer.skip_literal(tok, ")")) {
			panic(") expected");
		}
		*r = newtok("string", buf);
		return true;
	}
	if (tokenizer.skip_literal(tok, "<")) {
		// <3eb019d0b33c1532f6f9b8f9b79faac9>
		char buf[33] = {};
		tokenizer.readset(tok, "0123456789abcdef", buf, sizeof(buf));
		tokenizer.skip_literal(tok, ">");
		*r = newtok("hex", buf);
		return true;
	}
	if (tokenizer.skip_literal(tok, "/")) {
		// /Font
		char buf[20] = {};
		if (!isalpha(tokenizer.peek(tok)) && tokenizer.peek(tok) != '_') {
			panic("expected a name");
		}
		size_t pos = 0;
		while (tokenizer.more(tok)) {
			int c = tokenizer.peek(tok);
			if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-') {
				break;
			}
			tokenizer.get(tok);
			buf[pos++] = c;
		}
		buf[pos++] = '\0';
		*r = newtok("id", buf);
		return true;
	}
	if (isdigit(tokenizer.peek(tok))) {
		char buf[32] = {};
		if (!tokenizer.num(tok, buf, sizeof(buf))) {
			panic("num failed");
		}
		*r = newtok("num", buf);
		return true;
	}
	tokenizer.dbg(tok);
	panic("?");
}
