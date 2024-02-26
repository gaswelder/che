#import parsebuf
#import nodes.c

nodes.node_t True = {.kind = nodes.T};
nodes.node_t False = {.kind = nodes.F};

pub nodes.node_t *read_statement(parsebuf.parsebuf_t *b) {
	if (parsebuf.tok(b, "type", " ")) {
		return read_typedef(b);
	}
	return read_union(b);
}

nodes.node_t *read_typedef(parsebuf.parsebuf_t *b) {
	nodes.node_t *e = nodes.new(nodes.TYPEDEF);
	nodes.tdef_t *t = e->payload;

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

nodes.node_t *read_union(parsebuf.parsebuf_t *b) {
	nodes.node_t *e = nodes.new(nodes.UNION);
	e->items[e->itemslen++] = read_expr(b);
	parsebuf.spaces(b);
	while (parsebuf.buf_skip(b, '|')) {
		parsebuf.spaces(b);
		e->items[e->itemslen++] = read_expr(b);
	}
	return e;
}

nodes.node_t *read_expr(parsebuf.parsebuf_t *b) {
	if (parsebuf.tok(b, "true", " ]")) return &True;
	if (parsebuf.tok(b, "false", " ]")) return &False;
	if (parsebuf.buf_peek(b) == '"') return read_string(b);
	if (parsebuf.buf_peek(b) == '[') return read_list(b);
	if (isdigit(parsebuf.buf_peek(b))) return read_number(b);
	return read_identifier(b);
}

nodes.node_t *read_identifier(parsebuf.parsebuf_t *b) {
	nodes.node_t *e = nodes.new(nodes.ID);
	char *p = e->payload;
	if (!parsebuf.id(b, p, 100)) {
		char tmp[100] = {};
		parsebuf.buf_fcontext(b, tmp, sizeof(tmp));
		panic("failed to parse: '%s", tmp);
	}
	return e;
}

nodes.node_t *read_number(parsebuf.parsebuf_t *b) {
	nodes.node_t *e = nodes.new(nodes.NUM);
	if (!parsebuf.num(b, e->payload, 100)) {
		panic("failed to read the number");
	}
	return e;
}

nodes.node_t *read_string(parsebuf.parsebuf_t *b) {
	nodes.node_t *e = nodes.new(nodes.STR);
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

nodes.node_t *read_list(parsebuf.parsebuf_t *b) {
	nodes.node_t *e = nodes.new(nodes.LIST);
	expect(b, '[');
	while (parsebuf.buf_more(b) && parsebuf.buf_peek(b) != ']') {
		e->items[e->itemslen++] = read_expr(b);
		if (!parsebuf.buf_skip(b, ',')) {
			break;
		} else {
			parsebuf.spaces(b);
		}
	}
	expect(b, ']');
	return e;
}
