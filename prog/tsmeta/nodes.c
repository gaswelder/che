pub enum {
	T = 1, F, NUM, STR, LIST, ID, UNION, TYPEDEF, TYPECALL
};

pub const char *kindstr(int kind) {
	switch (kind) {
		case T: { return "TRUE"; }
		case F: { return "F"; }
		case NUM: { return "NUM"; }
		case STR: { return "STR"; }
		case LIST: { return "LIST"; }
		case ID: { return "ID"; }
		case UNION: { return "UNION"; }
		case TYPEDEF: { return "TYPEDEF"; }
		case TYPECALL: { return "TYPECALL"; }
		default: { return "unknown kind"; }
	}
}

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

pub typedef {
	char name[10];
	void *args[10];
	size_t nargs;
} tcall_t;

pub node_t *new(int kind) {
	node_t *e = calloc(1, sizeof(node_t));
	e->kind = kind;
	switch (kind) {
		case TYPEDEF: { e->payload = calloc(1, sizeof(tdef_t)); }
		case TYPECALL: { e-> payload = calloc(1, sizeof(tcall_t)); }
		default: { e->payload = calloc(100, 1); }
	}
	return e;
}
