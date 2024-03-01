#import parsebuf
#import strbuilder
#import nodes.c
#import format.c
#import read.c

nodes.node_t *global_types[100] = {};
size_t global_typeslen = 0;

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
	test("type Wrap<T> = [T]");
	test("Wrap<1>");
	test("type F<T, U> = [1]");
	return 0;
}

void test(const char *in) {
	// Read the expression.
	read.lexer_t *l = calloc(1, sizeof(read.lexer_t));
	l->b = parsebuf.buf_new(in);
	nodes.node_t *e = read.read_statement(l);

	// Print the input.
	strbuilder.str *s = strbuilder.str_new();
	format.format_expr(e, s);
	printf("> %s\n", strbuilder.str_raw(s));

	// Evaluate and print the result.
	nodes.node_t *r = eval(e);
	strbuilder.clear(s);
	format.format_expr(r, s);
	printf("%s\n", strbuilder.str_raw(s));
}

nodes.node_t *eval(nodes.node_t *e) {
	// printf("eval %s\n", nodes.kindstr(e->kind));
	switch (e->kind) {
		case nodes.T, nodes.F, nodes.NUM, nodes.STR: {
			return e;
		}
		case nodes.LIST: {
			nodes.node_t *r = nodes.new(nodes.LIST);
			for (size_t i = 0; i < e->itemslen; i++) {
				r->items[i] = eval(e->items[i]);
			}
			r->itemslen = e->itemslen;
			return r;
		}
		case nodes.TYPEDEF: {
			global_types[global_typeslen++] = e;
			return e;
		}
		case nodes.UNION: {
			nodes.node_t *r = nodes.new(nodes.UNION);
			for (size_t i = 0; i < e->itemslen; i++) {
				r->items[i] = eval(e->items[i]);
			}
			r->itemslen = e->itemslen;
			return r;
		}
		case nodes.TYPECALL: {
			nodes.tcall_t *call = e->payload;
			nodes.tdef_t *t = get_tdef(call->name);
			if (!t) {
				panic("unknown identifier: %s", call->name);
			}
			for (size_t i = 0; i < t->nargs; i++) {
				nodes.node_t *arg = nodes.new(nodes.TYPEDEF);
				nodes.tdef_t *x = arg->payload;
				strcpy(x->name, t->args[i]);
				x->expr = call->args[i];
				global_types[global_typeslen++] = arg;
			}
			nodes.node_t *r = eval(t->expr);
			for (size_t i = 0; i < t->nargs; i++) {
				global_typeslen--;
			}
			return r;
		}
		case nodes.ID: {
			char *name = e->payload;
			nodes.tdef_t *t = get_tdef(name);
			if (!t) {
				panic("unknown identifier: %s", name);
			}
			return t->expr;
		}
		default: {
			panic("don't know how to eval kind %d", e->kind);
		}
	}
	return e;
}

nodes.tdef_t *get_tdef(const char *name) {
	for (size_t i = 0; i < global_typeslen; i++) {
		nodes.tdef_t *t = global_types[global_typeslen-1-i]->payload;
		// printf("?%s -- %zu: %s\n", name, global_typeslen-1-i, t->name);
		if (!strcmp(t->name, name)) {
			return t;
		}
	}
	return NULL;
}
