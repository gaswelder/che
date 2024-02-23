#import parsebuf

enum {
	T = 1, F, NUM, STR, LIST
};

typedef {
	int kind;
	char payload[100];
	void *items[100];
	size_t itemslen;
} value_t;

typedef {
	value_t *list[100];
	int size;
} expr_t;

value_t True = {.kind = T};
value_t False = {.kind = F};

int main() {
	test("true");
	test("false");
	test("10");
	test("10.3");
	test("\"a\"");
	test("[1, \"a\"]");
	test("[1, \"a\", true]");
	test("1 | 2");
	return 0;
}

void test(const char *in) {
	parsebuf.parsebuf_t *b = parsebuf.buf_new(in);
	expr_t *e = read_expr(b);
	format_expr(e);
}

expr_t *read_expr(parsebuf.parsebuf_t *b) {
	expr_t *e = calloc(1, sizeof(expr_t));
	e->list[e->size++] = read_value(b);
	parsebuf.spaces(b);
	while (parsebuf.buf_skip(b, '|')) {
		parsebuf.spaces(b);
		e->list[e->size++] = read_value(b);
	}
	return e;
}

value_t *read_value(parsebuf.parsebuf_t *b) {
	if (parsebuf.tok(b, "true", " ]")) {
		return &True;
	}
	if (parsebuf.tok(b, "false", " ]")) {
		return &False;
	}
	if (isdigit(parsebuf.buf_peek(b))) {
		return read_number(b);
	}
	if (parsebuf.buf_peek(b) == '"') {
		return read_string(b);
	}
	if (parsebuf.buf_peek(b) == '[') {
		return read_list(b);
	}
	char tmp[100] = {};
	parsebuf.buf_fcontext(b, tmp, sizeof(tmp));
	panic("failed to parse: '%s", tmp);
}

value_t *read_number(parsebuf.parsebuf_t *b) {
	value_t *e = calloc(1, sizeof(value_t));
	char *p = e->payload;
	while (isdigit(parsebuf.buf_peek(b))) {
		*p++ = parsebuf.buf_get(b);
	}
	if (parsebuf.buf_peek(b) == '.') {
		*p++ = parsebuf.buf_get(b);
		while (isdigit(parsebuf.buf_peek(b))) {
			*p++ = parsebuf.buf_get(b);
		}
	}
	e->kind = NUM;
	return e;
}

value_t *read_string(parsebuf.parsebuf_t *b) {
	value_t *e = calloc(1, sizeof(value_t));
	char *p = e->payload;
	*p++ = parsebuf.buf_get(b);
	while (parsebuf.buf_more(b) && parsebuf.buf_peek(b) != '"') {
		*p++ = parsebuf.buf_get(b);
	}
	if (parsebuf.buf_peek(b) != '"') {
		panic("expected closing quote");
	}
	*p++ = parsebuf.buf_get(b);
	e->kind = STR;
	return e;
}

char expect(parsebuf.parsebuf_t *b, char c) {
	if (parsebuf.buf_peek(b) != c) {
		panic("expected %c", c);
	}
	return parsebuf.buf_get(b);
}

value_t *read_list(parsebuf.parsebuf_t *b) {
	value_t *e = calloc(1, sizeof(value_t));
	expect(b, '[');
	while (parsebuf.buf_more(b) && parsebuf.buf_peek(b) != ']') {
		e->items[e->itemslen++] = read_value(b);
		if (!parsebuf.buf_skip(b, ',')) {
			break;
		} else {
			parsebuf.spaces(b);
		}
	}
	expect(b, ']');
	e->kind = LIST;
	return e;
}

void format_expr(expr_t *e) {
	char buf[1000] = {};
	size_t n = 0;

	for (int i = 0; i < e->size; i++) {
		n += format_atom(e->list[i], buf + n, 1000 - n);
		if (i + 1 < e->size) {
			n += snprintf(buf + n, 1000-n, " | ");
		}
	}
	printf("%s\n", buf);
}

int format_atom(value_t *e, char *buf, size_t n) {
	switch (e->kind) {
		case T: { return snprintf(buf, n, "%s", "true"); }
		case F: { return snprintf(buf, n, "%s", "false"); }
		case NUM: { return snprintf(buf, n, "%s", e->payload); }
		case STR: { return snprintf(buf, n, "%s", e->payload); }
		case LIST: {
			int r = 0;
			r += snprintf(buf + r, n-r, "[");
			for (size_t i = 0; i < e->itemslen; i++) {
				if (i > 0) {
					r += snprintf(buf + r, n-r, ", ");
				}
				r += format_atom(e->items[i], buf + r, n - r);
			}
			r += snprintf(buf + r, n-r, "]");
			return r;
		}
		default: {
			panic("format: unknown kind");
		}
	}
}
