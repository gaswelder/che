#import tok.c

typedef {
	char name[80];
	tok.tok_t *val;
} def_t;

size_t ndefs = 0;
def_t defs[100] = {};

typedef {
	char name[80];
	char argname[10];
	tok.tok_t *body;
} fn_t;

size_t nfns = 0;
fn_t fns[100] = {};

void pushdef(const char *name, tok.tok_t *val) {
	strcpy(defs[ndefs].name, name);
	defs[ndefs].val = val;
	ndefs++;
}

pub tok.tok_t *evalall(tok.tok_t **all) {
	tok.tok_t *r = NULL;
	size_t n = 0;
	tok.tok_t *x = all[n++];
	while (x) {
		r = eval(x);
		x = all[n++];
	}
	return r;
}


// Evaluates a node.
pub tok.tok_t *eval(tok.tok_t *x) {
	switch (x->type) {
		case tok.SYMBOL: { return eval_symbol(x); }
		case tok.LIST: { return eval_list(x); }
		case tok.NUMBER: { return x; }
		default: {
			panic("unexpected node type: %d", x->type);
		}
	}
}

// Looks up a symbol.
tok.tok_t *eval_symbol(tok.tok_t *x) {
	tok.tok_t *r = NULL;
	for (size_t i = 0; i < ndefs; i++) {
		if (!strcmp(defs[i].name, x->name)) {
			r = defs[i].val;
		}
	}
	return r;
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
		case "*": { return mul(args); }
		case "+": { return add(args); }
		case "-": { return sub(args); }
		case "/": { return over(args); }
	}

	// See if there is a defined function with this name.
	for (size_t i = 0; i < nfns; i++) {
		if (!strcmp(name, fns[i].name)) {
			pushdef("x", car(args));
			tok.tok_t *r = eval(fns[i].body);
			ndefs--;
			return r;
		}
	}


	// if (name == rt.intern("cond")) return cond(args);
	panic("unknown function %s", name);
	return NULL;
}

// (define x const) defines a constant.
// (define (f x) body) defines a function.
tok.tok_t *define(tok.tok_t *args) {
	tok.tok_t *def = car(args);
	tok.tok_t *val = car(cdr(args));
	if (def->type == tok.SYMBOL) {
		pushdef(def->name, val);
		return NULL;
	}

	// (twice x) (* x 2)
	if (def->type == tok.LIST) {
		tok.tok_t *name = car(def);
		tok.tok_t *arg = car(cdr(def));
		strcpy(fns[nfns].name, name->name);
		strcpy(fns[nfns].argname, arg->name);
		fns[nfns].body = val;
		nfns++;
		return NULL;
	}

	tok.dbgprint(args);
	panic("unknown define shape");
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

// (* a b) returns a * b
tok.tok_t *mul(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));
	if (a->type != tok.NUMBER || b->type != tok.NUMBER) {
		panic("not a number");
	}
	char buf[100];
	sprintf(buf, "%d", atoi(a->value) * atoi(b->value));
	return tok.newnumber(buf);
}

// (+ a b) returns a + b
tok.tok_t *add(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));
	if (a->type != tok.NUMBER || b->type != tok.NUMBER) {
		panic("not a number");
	}
	char buf[100];
	sprintf(buf, "%d", atoi(a->value) + atoi(b->value));
	return tok.newnumber(buf);
}

// (- a b) returns a - b
tok.tok_t *sub(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));
	if (a->type != tok.NUMBER || b->type != tok.NUMBER) {
		panic("not a number");
	}
	char buf[100];
	sprintf(buf, "%d", atoi(a->value) - atoi(b->value));
	return tok.newnumber(buf);
}

// (/ a b) returns a / b
tok.tok_t *over(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));
	if (a->type != tok.NUMBER || b->type != tok.NUMBER) {
		panic("not a number");
	}
	char buf[100];
	sprintf(buf, "%d", atoi(a->value) / atoi(b->value));
	return tok.newnumber(buf);
}



// (eq? a b) returns true if a equals b.
tok.tok_t *eq(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));

	// If types don't match, then not equal.
	if (a->type != b->type) {
		return NULL;
	}
	bool same = false;
	switch (a->type) {
		case tok.SYMBOL: { same = !strcmp(a->name, b->name); }
		case tok.NUMBER: { same = !strcmp(a->value, b->value); }
		default: {
			panic("unhandled item type: %d", a->type);
		}
	}
	if (same) {
		return tok.newsym("true");
	}
	return NULL;
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
