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
} node_t;

node_t True = {.kind = T};
node_t False = {.kind = F};

node_t *node(int kind) {
	node_t *e = calloc(1, sizeof(node_t));
	e->kind = kind;
	return e;
}

int main() {
	test("true");
	test("false");
	test("10");
	test("10.3");
	test("\"a\"");
	test("[1, \"a\"]");
	test("[1, \"a\", true]");
	test("1 | 2");
	test("type Wrap<T> = [1]");
	// Wrap<true>
	return 0;
}

void test(const char *in) {
	parsebuf.parsebuf_t *b = parsebuf.buf_new(in);
	node_t *e = read_statement(b);

	strbuilder.str *s = strbuilder.str_new();
	format_expr(e, s);
	printf("%s\n", strbuilder.str_raw(s));
}

node_t *read_statement(parsebuf.parsebuf_t *b) {
	if (parsebuf.tok(b, "type", " ")) {
		return read_typedef(b);
	}
	return read_union(b);
}

node_t *read_typedef(parsebuf.parsebuf_t *b) {
	node_t *e = node(TYPEDEF);

	parsebuf.spaces(b);
	if (!parsebuf.id(b, e->name, sizeof(e->name))) {
		panic("failed to read name");
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
	e->arg[0] = param;
	e->expr = read_union(b);
	return e;
}

node_t *read_union(parsebuf.parsebuf_t *b) {
	node_t *e = node(UNION);
	e->items[e->itemslen++] = read_value(b);
	parsebuf.spaces(b);
	while (parsebuf.buf_skip(b, '|')) {
		parsebuf.spaces(b);
		e->items[e->itemslen++] = read_value(b);
	}
	return e;
}

node_t *read_value(parsebuf.parsebuf_t *b) {
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

node_t *read_number(parsebuf.parsebuf_t *b) {
	node_t *e = node(NUM);
	if (!parsebuf.num(b, e->payload, sizeof(e->payload))) {
		panic("failed to read the number");
	}
	return e;
}

node_t *read_string(parsebuf.parsebuf_t *b) {
	node_t *e = node(STR);
	char *p = e->payload;
	*p++ = parsebuf.buf_get(b);
	while (parsebuf.buf_more(b) && parsebuf.buf_peek(b) != '"') {
		*p++ = parsebuf.buf_get(b);
	}
	if (parsebuf.buf_peek(b) != '"') {
		panic("expected closing quote");
	}
	*p++ = parsebuf.buf_get(b);
	return e;
}

char expect(parsebuf.parsebuf_t *b, char c) {
	if (parsebuf.buf_peek(b) != c) {
		panic("expected %c", c);
	}
	return parsebuf.buf_get(b);
}

node_t *read_list(parsebuf.parsebuf_t *b) {
	node_t *e = node(LIST);
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
	return e;
}

void format_expr(node_t *e, strbuilder.str *s) {
	switch (e->kind) {
		case UNION: { format_union(e, s); }
		case TYPEDEF: {
			strbuilder.addf(s, "type %s<%s> = ", e->name, e->arg);
			format_expr(e->expr, s);
		}
		default: {
			panic("unknown expr kind: %d", e->kind);
		}
	}
}

void format_union(node_t *e, strbuilder.str *s) {
	char buf[1000] = {};
	size_t n = 0;
	for (size_t i = 0; i < e->itemslen; i++) {
		n += format_atom(e->items[i], buf + n, 1000 - n);
		if (i + 1 < e->itemslen) {
			n += snprintf(buf + n, 1000-n, " | ");
		}
	}
	strbuilder.adds(s, buf);
}

int format_atom(node_t *e, char *buf, size_t n) {
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
