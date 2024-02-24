#import parsebuf
#import strbuilder

enum {
	T = 1, F, NUM, STR, LIST, ID, UNION, TYPEDEF
};

typedef {
	int kind;
	void *payload;

	// for list
	void *items[100];
	size_t itemslen;
} node_t;

typedef {
	char name[10];
	char *args[10];
	size_t nargs;
	void *expr;
} tdef_t;

node_t True = {.kind = T};
node_t False = {.kind = F};

node_t *node(int kind) {
	node_t *e = calloc(1, sizeof(node_t));
	e->kind = kind;
	if (kind == TYPEDEF) {
		e->payload = calloc(1, sizeof(tdef_t));
	} else {
		e->payload = calloc(100, 1);
	}
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
	test("type A = 1");
	test("A");
	test("type Wrap<T, U> = [1]");
	// Wrap<true>
	return 0;
}

node_t *global_types[100] = {};
size_t global_typeslen = 0;

void test(const char *in) {
	// Read the expression.
	parsebuf.parsebuf_t *b = parsebuf.buf_new(in);
	node_t *e = read_statement(b);

	// Print the input.
	strbuilder.str *s = strbuilder.str_new();
	format_expr(e, s);
	printf("> %s\n", strbuilder.str_raw(s));

	// Evaluate and print the result.
	node_t *r = eval(e);
	strbuilder.clear(s);
	format_expr(r, s);
	printf("%s\n", strbuilder.str_raw(s));
}

node_t *eval(node_t *e) {
	switch (e->kind) {
		case T, F, NUM, STR, LIST: {
			return e;
		}
		case TYPEDEF: {
			global_types[global_typeslen++] = e;
			return e;
		}
		case UNION: {
			node_t *r = node(UNION);
			for (size_t i = 0; i < e->itemslen; i++) {
				r->items[i] = eval(e->items[i]);
			}
			r->itemslen = e->itemslen;
			return r;
		}
		case ID: {
			char *name = e->payload;
			for (size_t i = 0; i < global_typeslen; i++) {
				tdef_t *t = global_types[i]->payload;
				if (!strcmp(t->name, name)) {
					return t->expr;
				}
			}
			panic("unknown identifier: %s", name);
			return NULL;
		}
		default: {
			panic("don't know how to eval kind %d", e->kind);
		}
	}
	return e;
}

node_t *read_statement(parsebuf.parsebuf_t *b) {
	if (parsebuf.tok(b, "type", " ")) {
		return read_typedef(b);
	}
	return read_union(b);
}

node_t *read_typedef(parsebuf.parsebuf_t *b) {
	node_t *e = node(TYPEDEF);
	tdef_t *t = e->payload;

	parsebuf.spaces(b);
	if (!parsebuf.id(b, t->name, sizeof(t->name))) {
		panic("failed to read name");
	}

	if (parsebuf.buf_skip(b, '<')) {
		while (parsebuf.buf_more(b)) {
			size_t n = t->nargs++;
			t->args[n] = calloc(1, 100);
			char param = parsebuf.buf_get(b);
			if (!isalpha(param)) {
				panic("expected an id, got '%c'", param);
			}
			t->args[n][0] = param;
			parsebuf.spaces(b);
			if (!parsebuf.buf_skip(b, ',')) {
				break;
			} else {
				parsebuf.spaces(b);
			}
		}
		expect(b, '>');
	}
	parsebuf.spaces(b);
	expect(b, '=');
	parsebuf.spaces(b);
	t->expr = read_union(b);
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
	if (parsebuf.tok(b, "true", " ]")) return &True;
	if (parsebuf.tok(b, "false", " ]")) return &False;
	if (parsebuf.buf_peek(b) == '"') return read_string(b);
	if (parsebuf.buf_peek(b) == '[') return read_list(b);
	if (isdigit(parsebuf.buf_peek(b))) return read_number(b);
	return read_identifier(b);
}

node_t *read_identifier(parsebuf.parsebuf_t *b) {
	node_t *e = node(ID);
	char *p = e->payload;
	if (!parsebuf.id(b, p, 100)) {
		char tmp[100] = {};
		parsebuf.buf_fcontext(b, tmp, sizeof(tmp));
		panic("failed to parse: '%s", tmp);
	}
	return e;
}

node_t *read_number(parsebuf.parsebuf_t *b) {
	node_t *e = node(NUM);
	if (!parsebuf.num(b, e->payload, 100)) {
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
		case T: { strbuilder.adds(s, "true"); }
		case F: { strbuilder.adds(s, "false"); }
		case NUM, STR, ID: { strbuilder.adds(s, e->payload); }
		case UNION: { format_union(e, s); }
		case TYPEDEF: { format_typedef(e, s); }
		case LIST: { format_list(e, s); }
		default: {
			panic("format_expr: unknown kind %d", e->kind);
		}
	}
}

void format_list(node_t *e, strbuilder.str *s) {
	strbuilder.adds(s, "[");
	for (size_t i = 0; i < e->itemslen; i++) {
		if (i > 0) strbuilder.adds(s, ", ");
		format_expr(e->items[i], s);
	}
	strbuilder.adds(s, "]");
}

void format_union(node_t *e, strbuilder.str *s) {
	for (size_t i = 0; i < e->itemslen; i++) {
		format_expr(e->items[i], s);
		if (i + 1 < e->itemslen) {
			strbuilder.adds(s, " | ");
		}
	}
}

void format_typedef(node_t *e, strbuilder.str *s) {
	tdef_t *t = e->payload;
	strbuilder.addf(s, "type %s", t->name);
	if (t->nargs > 0) {
		strbuilder.adds(s, "<");
		for (size_t i = 0; i < t->nargs; i++) {
			if (i > 0) strbuilder.adds(s, ", ");
			strbuilder.adds(s, t->args[i]);
		}
		strbuilder.adds(s, ">");
	}
	strbuilder.adds(s, " = ");
	format_expr(t->expr, s);
}

