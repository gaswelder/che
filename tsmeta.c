#import parsebuf
#import strbuilder

enum {
	T = 1, F, NUM, STR, LIST, UNION, TYPEDEF
};

typedef {
	int kind;
	char payload[100];

	// for list
	void *items[100];
	size_t itemslen;

	// typedef
	char name[10];
	char arg[10];
	void *expr;
} value_t;

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
	test("type F<T> = [1]");
	// Wrap<true>
	return 0;
}

void test(const char *in) {
	parsebuf.parsebuf_t *b = parsebuf.buf_new(in);
	value_t *e = read_statement(b);

	strbuilder.str *s = strbuilder.str_new();
	format_expr(e, s);
	printf("%s\n", strbuilder.str_raw(s));
}

value_t *read_statement(parsebuf.parsebuf_t *b) {
	if (parsebuf.tok(b, "type", " ")) {
		parsebuf.spaces(b);
		char name = parsebuf.buf_get(b);
		if (!isalpha(name)) {
			panic("expected an id, got '%c'", name);
		}
		expect(b, '<');
		char param = parsebuf.buf_get(b);
		if (!isalpha(param)) {
			panic("expected an id, got '%c'", param);
		}
		expect(b, '>');
		parsebuf.spaces(b);
		expect(b, '=');
		parsebuf.spaces(b);

		value_t *e = calloc(1, sizeof(value_t));
		e->kind = TYPEDEF;
		e->name[0] = name;
		e->arg[0] = param;
		e->expr = read_union(b);
		return e;
	}
	return read_union(b);
}

value_t *read_union(parsebuf.parsebuf_t *b) {
	value_t *e = calloc(1, sizeof(value_t));
	e->kind = UNION;
	e->items[e->itemslen++] = read_value(b);
	parsebuf.spaces(b);
	while (parsebuf.buf_skip(b, '|')) {
		parsebuf.spaces(b);
		e->items[e->itemslen++] = read_value(b);
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

void format_expr(value_t *e, strbuilder.str *s) {
	char buf[1000] = {};
	size_t n = 0;

	switch (e->kind) {
		case UNION: {
			for (size_t i = 0; i < e->itemslen; i++) {
				n += format_atom(e->items[i], buf + n, 1000 - n);
				if (i + 1 < e->itemslen) {
					n += snprintf(buf + n, 1000-n, " | ");
				}
			}
		}
		case TYPEDEF: {
			n += snprintf(buf + n, 1000-n, "type %s<%s> = (...)", e->name, e->arg);
		}
		default: {
			panic("unknown expr kind: %d", e->kind);
		}
	}
	strbuilder.adds(s, buf);
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
