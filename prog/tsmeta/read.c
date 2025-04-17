#import parsebuf
#import nodes.c

nodes.node_t True = {.kind = nodes.T};
nodes.node_t False = {.kind = nodes.F};

pub nodes.node_t *read_statement(lexer_t *l) {
	tok_t *t = lex_peektok(l);
	if (t && t->kind == TOK_ID && !strcmp(t->payload, "type")) {
		return read_typedef(l);
	}
	return read_union(l);
}

nodes.node_t *read_typedef(lexer_t *l) {
	lex_pop(l);

	nodes.node_t *node = nodes.new(nodes.TYPEDEF);
	nodes.tdef_t *tdef = node->payload;

	tok_t *tok = expect(l, TOK_ID);
	strcpy(tdef->name, tok->payload);

	if (lex_get(l, '<')) {
		while (lex_more(l)) {
			tok_t *tok = expect(l, TOK_ID);

			size_t n = tdef->nargs++;
			tdef->args[n] = calloc(1, 100);
			strcpy(tdef->args[n], tok->payload);

			if (!lex_get(l, ',')) {
				break;
			}
		}
		expect(l, '>');
	}
	expect(l, '=');
	tdef->expr = read_union(l);
	return node;
}

// <expr> | <expr> | ...
nodes.node_t *read_union(lexer_t *l) {
	nodes.node_t *node = nodes.new(nodes.UNION);
	node->items[node->itemslen++] = read_atom(l);
	while (lex_get(l, '|')) {
		node->items[node->itemslen++] = read_atom(l);
	}
	return node;
}

nodes.node_t *read_atom(lexer_t *l) {
	if (lex_peek(l) == TOK_STRING) {
		tok_t *t = lex_get(l, TOK_STRING);
		nodes.node_t *e = nodes.new(nodes.STR);
		strcpy(e->payload, t->payload);
		return e;
	}
	if (lex_peek(l) == TOK_NUMBER) {
		tok_t *t = lex_get(l, TOK_NUMBER);
		nodes.node_t *e = nodes.new(nodes.NUM);
		strcpy(e->payload, t->payload);
		return e;
	}
	if (lex_peek(l) == '[') {
		nodes.node_t *e = nodes.new(nodes.LIST);
		lex_get(l, '[');
		while (lex_more(l) && lex_peek(l) != ']') {
			e->items[e->itemslen++] = read_atom(l);
			if (!lex_get(l, ',')) {
				break;
			}
		}
		expect(l, ']');
		return e;
	}
	if (lex_peek(l) == TOK_ID) {
		tok_t *t = lex_get(l, TOK_ID);
		char *name = t->payload;
		if (!strcmp(name, "true")) return &True;
		if (!strcmp(name, "false")) return &False;
		return read_typecall(l, name);
	}

	panic("failed to parse");
	return NULL;
}

nodes.node_t *read_typecall(lexer_t *l, const char *name) {
	nodes.node_t *e = nodes.new(nodes.TYPECALL);
	nodes.tcall_t *t = e->payload;
	strcpy(t->name, name);
	if (lex_get(l, '<')) {
		while (lex_more(l)) {
			t->args[t->nargs++] = read_atom(l);
			if (!lex_get(l, ',')) {
				break;
			}
		}
		expect(l, '>');
	}
	return e;
}

tok_t *expect(lexer_t *l, int kind) {
	if (lex_peek(l) != kind) {
		panic("expected %d (%c)", kind, kind);
	}
	return lex_get(l, kind);
}

enum {
	TOK_NUMBER,
	TOK_STRING,
	TOK_ID
};

pub typedef {
	int kind;
	char payload[100];
} tok_t;

pub typedef {
	parsebuf.parsebuf_t *b;
	tok_t *next;
} lexer_t;

int lex_peek(lexer_t *l) {
	if (!l->next && parsebuf.more(l->b)) {
		l->next = lex_read(l);
	}
	if (!l->next) {
		return -1;
	}
	return l->next->kind;
}

tok_t *lex_peektok(lexer_t *l) {
	if (!l->next && parsebuf.more(l->b)) {
		l->next = lex_read(l);
	}
	return l->next;
}

tok_t *lex_get(lexer_t *l, int kind) {
	if (!l->next && parsebuf.more(l->b)) {
		l->next = lex_read(l);
	}
	if (!l->next || l->next->kind != kind) {
		return NULL;
	}
	tok_t *t = l->next;
	l->next = NULL;
	return t;
}

bool lex_more(lexer_t *l) {
	return l->next || parsebuf.more(l->b);
}

void lex_pop(lexer_t *l) {
	if (!l->next && parsebuf.more(l->b)) {
		l->next = lex_read(l);
	}
	if (l->next) {
		l->next = NULL;
	}
}

tok_t *lex_read(lexer_t *l) {
	parsebuf.parsebuf_t *b = l->b;
	parsebuf.spaces(b);
	tok_t *t = calloc(1, sizeof(tok_t));
	int c = parsebuf.peek(b);

	if (isdigit(c)) {
		if (!parsebuf.num(b, t->payload, 100)) {
			panic("failed to read the number");
		}
		t->kind = TOK_NUMBER;
		// printf("read token: (%d, %s)\n", t->kind, t->payload);
		return t;
	}
	if (isalpha(c)) {
		parsebuf.id(b, t->payload, 100);
		t->kind = TOK_ID;
		// printf("read token: (%d, %s)\n", t->kind, t->payload);
		return t;
	}
	if (c == '[' || c == ']' || c == '|' || c == '<' || c == '>' || c == ',' || c == '=') {
		parsebuf.get(b);
		t->kind = c;
		// printf("read token: (%d, %s)\n", t->kind, t->payload);
		return t;
	}
	if (c == '"') {
		char *p = t->payload;
		*p++ = parsebuf.get(b);
		while (parsebuf.more(b) && parsebuf.peek(b) != '"') {
			*p++ = parsebuf.get(b);
		}
		if (parsebuf.peek(b) != '"') {
			panic("expected closing quote");
		}
		*p++ = parsebuf.get(b);
		t->kind = TOK_STRING;
		// printf("read token: (%d, %s)\n", t->kind, t->payload);
		return t;
	}
	panic("lexer: got '%c' (%d)", c, c);
}
