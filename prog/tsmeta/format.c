#import strbuilder
#import nodes.c

pub void format_expr(nodes.node_t *e, strbuilder.str *s) {
	switch (e->kind) {
		case nodes.T: { strbuilder.adds(s, "true"); }
		case nodes.F: { strbuilder.adds(s, "false"); }
		case nodes.NUM, nodes.STR, nodes.ID: { strbuilder.adds(s, e->payload); }
		case nodes.UNION: { format_union(e, s); }
		case nodes.TYPEDEF: { format_typedef(e, s); }
		case nodes.TYPECALL: { format_typecall(e, s); }
		case nodes.LIST: { format_list(e, s); }
		default: {
			panic("format_expr: unknown kind %d", e->kind);
		}
	}
}

void format_list(nodes.node_t *e, strbuilder.str *s) {
	strbuilder.adds(s, "[");
	for (size_t i = 0; i < e->itemslen; i++) {
		if (i > 0) strbuilder.adds(s, ", ");
		format_expr(e->items[i], s);
	}
	strbuilder.adds(s, "]");
}

void format_union(nodes.node_t *e, strbuilder.str *s) {
	for (size_t i = 0; i < e->itemslen; i++) {
		format_expr(e->items[i], s);
		if (i + 1 < e->itemslen) {
			strbuilder.adds(s, " | ");
		}
	}
}

void format_typedef(nodes.node_t *e, strbuilder.str *s) {
	nodes.tdef_t *t = e->payload;
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

void format_typecall(nodes.node_t *e, strbuilder.str *s) {
	nodes.tcall_t *t = e->payload;
	strbuilder.adds(s, t->name);
	if (t->nargs > 0) {
		strbuilder.adds(s, "<");
		for (size_t i = 0; i < t->nargs; i++) {
			if (i > 0) strbuilder.adds(s, ", ");
			format_expr(t->args[i], s);
		}
		strbuilder.adds(s, ">");
	}
}