pub enum {
	T = 1, F, NUM, STR, LIST, ID, UNION, TYPEDEF
};

pub typedef {
	int kind;
	void *payload;

	// for list
	void *items[100];
	size_t itemslen;
} node_t;

pub typedef {
	char name[10];
	char *args[10];
	size_t nargs;
	void *expr;
} tdef_t;

pub node_t *new(int kind) {
	node_t *e = calloc(1, sizeof(node_t));
	e->kind = kind;
	if (kind == TYPEDEF) {
		e->payload = calloc(1, sizeof(tdef_t));
	} else {
		e->payload = calloc(100, 1);
	}
	return e;
}
