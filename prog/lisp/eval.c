#import tok.c

typedef {
	char name[80];
	tok.tok_t *val;
} def_t;

size_t ndefs = 0;
def_t defs[100] = {};


// Evaluates a node.
pub tok.tok_t *eval(tok.tok_t *x) {
	switch (x->type) {
		case tok.LIST: { return eval_list(x); }
		case tok.SYMBOL: { return x; }
		default: {
			panic("unexpected node type: %d", x->type);
		}
	}
}

// Evaluates a list node.
tok.tok_t *eval_list(tok.tok_t *x) {
	tok.tok_t *first = car(x);
	if (first->type != tok.SYMBOL) {
		tok.dbgprint(x);
		panic("first element is a non-symbol");
	}
	return runfunc(first->name, cdr(x));
}

// Runs a function.
tok.tok_t *runfunc(const char *name, tok.tok_t *args) {
	switch str (name) {
		case "apply": { return apply(args); }
		case "eq?": { return eq(args); }
		case "quote": { return car(args); }
		case "cons": { return cons(args); }
		case "define": { return define(args); }
	}
	// if (name == rt.intern("cond")) return cond(args);
	panic("unknown function %s", name);
	return NULL;
}

tok.tok_t *define(tok.tok_t *args) {
	tok.tok_t *name = car(args);
	tok.tok_t *val = car(cdr(args));
	strcpy(defs[ndefs].name, name->name);
	defs[ndefs].val = val;
	ndefs++;
	return NULL;
}


// rt.item_t *cond(rt.item_t *args) {
// 	rt.item_t *l = args;
// 	while (l) {
// 		rt.item_t *cas = rt.car(l);
// 		rt.item_t *cond = rt.car(cas);
// 		if (eval(cond)) {
// 			rt.item_t *result = rt.car(rt.cdr(cas));
// 			return eval(result);
// 		}
// 		l = rt.cdr(l);
// 	}
// 	return NULL;
// }

tok.tok_t *apply(tok.tok_t *list) {
	tok.tok_t *fn = car(list);
	if (fn->type != tok.SYMBOL) {
		tok.dbgprint(list);
		panic("first element is a non-symbol");
	}
	return runfunc(fn->name, eval(car(cdr(list))));
}



// Implements the eq? function.
tok.tok_t *eq(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));

	// If types don't match, then not equal.
	if (a->type != b->type) {
		return NULL;
	}
	switch (a->type) {
		// Compare symbols by their content.
		case tok.SYMBOL: {
			if (!strcmp(a->name, b->name)) {
				return tok.newsym("true");
			}
			return NULL;
		}
	}
	panic("unhandled item type: %d", a->type);
}



// (cons 1 x) constructs a list (1, ...x)
tok.tok_t *cons(tok.tok_t *args) {
	tok.tok_t *head = car(args);
	tok.tok_t *tail = car(cdr(args));

	tok.tok_t *r = tok.newlist();
	r->items[r->nitems++] = head;
	for (size_t i = 0; i < tail->nitems; i++) {
		r->items[r->nitems++] = tail->items[i];
	}
	return r;
}

// Returns the first item of the list x.
tok.tok_t *car(tok.tok_t *x) {
	if (x->type != tok.LIST || x->nitems == 0) {
		return NULL;
	}
	return x->items[0];
}

// Returns the tail of the list x.
tok.tok_t *cdr(tok.tok_t *x) {
	if (!x || x->type != tok.LIST || x->nitems <= 1) {
		return NULL;
	}
	tok.tok_t *r = tok.newlist();
	for (size_t i = 1; i < x->nitems; i++) {
		r->items[i-1] = x->items[i];
	}
	r->nitems = x->nitems-1;
	return r;
}
