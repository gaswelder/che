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
	parsebuf.parsebuf_t *b = parsebuf.buf_new(in);
	nodes.node_t *e = read.read_statement(b);

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
	switch (e->kind) {
		case nodes.T, nodes.F, nodes.NUM, nodes.STR, nodes.LIST: {
			return e;
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
		nodes.tdef_t *t = global_types[i]->payload;
		if (!strcmp(t->name, name)) {
			return t;
		}
	}
	return NULL;
}
