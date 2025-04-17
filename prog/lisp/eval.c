#import tok.c

typedef {
	bool isfunc;

	// Name of the function or constant.
	char name[80];

	// Argument names, if it's a function.
	size_t nargs;
	char argnames[10][10];

	// Constant value or function body.
	tok.tok_t *val;
} def_t;

size_t ndefs = 0;
def_t defs[100] = {};

def_t *lookup(const char *name) {
	for (size_t i = 0; i < ndefs; i++) {
		if (!strcmp(name, defs[i].name)) {
			return &defs[i];
		}
	}
	return NULL;
}

void pushdef(const char *name, tok.tok_t *val) {
	strcpy(defs[ndefs].name, name);
	defs[ndefs].val = val;
	ndefs++;
}

// void printfn(fn_t *f) {
// 	printf("fn %s(", f->name);
// 	for (size_t i = 0; i < f->nargs; i++) {
// 		if (i > 0) printf(" ");
// 		printf("%s", f->argnames[i]);
// 	}
// 	printf(")");
// }

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

bool trace = false;
size_t depth = 0;

// Evaluates a node.
pub tok.tok_t *eval(tok.tok_t *x) {
	if (x->type == tok.NUMBER) return x;
	if (x->type == tok.SYMBOL) {
		tok.tok_t *r = eval_symbol(x);
		if (trace) {
			for (size_t i = 0; i < depth; i++) printf("  ");
			printf("%s: ", x->name);
			tok.dbgprint(r);
		}
		return r;
	}
	depth++;
	if (depth == 100) {
		panic("eval stack overflow (%zu)", depth);
	}
	if (trace) {
		for (size_t i = 0; i < depth; i++) printf("  ");
		printf("eval: ");
		tok.dbgprint(x);
	}
	tok.tok_t *r = NULL;
	r = eval_list(x);
	if (trace) {
		for (size_t i = 0; i < depth; i++) printf("  ");
		printf("result: ");
		tok.dbgprint(r);
	}
	depth--;
	return r;
}

// Looks up a symbol.
tok.tok_t *eval_symbol(tok.tok_t *x) {
	// if (!strcmp(x->name, "true") || !strcmp(x->name, "+") || !strcmp(x->name, "-")) {
	// 	return x;
	// }
	tok.tok_t *r = NULL;
	for (size_t i = 0; i < ndefs; i++) {
		if (!strcmp(defs[i].name, x->name)) {
			r = defs[i].val;
		}
	}
	if (!r) return x;
	return r;
}

// Evaluates a list node.
tok.tok_t *eval_list(tok.tok_t *x) {
	tok.tok_t *first = car(x);
	if (first->type != tok.SYMBOL) {
		first = eval(first);
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
		case ">": { return gt(args); }
		case "<": { return lt(args); }
		case "=": { return numeq(args); }
		case "cond": { return cond(args); }
		case "if": { return fif(args); }
		case "and": { return and(args); }
		case "or": { return or(args); }
		case "not": { return not(args); }
	}

	// See if there is a defined function with this name.
	def_t *f = lookup(name);
	if (!f) {
		panic("unknown function %s", name);
	}
	if (!f->isfunc) {
		panic("%s is not a function", name);
	}

	for (size_t a = 0; a < f->nargs; a++) {
		pushdef(f->argnames[a], eval(args->items[a]));
	}
	tok.tok_t *r = eval(f->val);
	ndefs -= f->nargs;
	return r;	
}

// (define x const) defines a constant.
// (define (f x) body) defines a function.
tok.tok_t *define(tok.tok_t *args) {
	tok.tok_t *def = car(args);
	tok.tok_t *val = car(cdr(args));
	if (def->type == tok.SYMBOL) {
		pushdef(def->name, eval(val));
		return NULL;
	}

	// (twice x) (* x 2)
	if (def->type == tok.LIST) {
		tok.tok_t *name = car(def);
		def_t *d = &defs[ndefs++];
		strcpy(d->name, name->name);
		d->isfunc = true;
		d->val = val;
		for (size_t i = 1; i < def->nitems; i++) {
			strcpy(d->argnames[d->nargs++], def->items[i]->name);
		}
		return NULL;
	}

	tok.dbgprint(args);
	panic("unknown define shape");
}


tok.tok_t *cond(tok.tok_t *args) {
	tok.tok_t *l = args;
	while (l) {
		tok.tok_t *cas = car(l);
		tok.tok_t *cond = car(cas);
		if (eval(cond)) {
			tok.tok_t *result = car(cdr(cas));
			return eval(result);
		}
		l = cdr(l);
	}
	return NULL;
}

tok.tok_t *not(tok.tok_t *args) {
	if (eval(car(args))) {
		return NULL;
	}
	return tok.newsym("true");
}

tok.tok_t *fif(tok.tok_t *args) {
	tok.tok_t *pred = car(args);
	tok.tok_t *ethen = car(cdr(args));
	if (eval(pred)) {
		return eval(ethen);
	}
	if (args->nitems < 3) {
		return NULL;
	}
	tok.tok_t *eelse = car(cdr(cdr(args)));
	return eval(eelse);
}

tok.tok_t *and(tok.tok_t *args) {
	for (size_t i = 0; i < args->nitems; i++) {
		if (!eval(args->items[i])) {
			return NULL;
		}
	}
	return tok.newsym("true");
}

tok.tok_t *or(tok.tok_t *args) {
	for (size_t i = 0; i < args->nitems; i++) {
		if (eval(args->items[i])) {
			return tok.newsym("true");
		}
	}
	return NULL;
}

tok.tok_t *apply(tok.tok_t *list) {
	tok.tok_t *fn = car(list);
	if (fn->type != tok.SYMBOL) {
		tok.dbgprint(list);
		panic("first element is a non-symbol");
	}
	return runfunc(fn->name, eval(car(cdr(list))));
}

void printnum(char *buf, double x) {
	sprintf(buf, "%g", x);
}

double reduce(tok.tok_t *args, double start, int op) {
	if (args->type != tok.LIST) {
		panic("not a list");
	}
	if (args->nitems < 2) {
		panic("want 2 or more arguments");
	}
	double r = start;
	for (size_t i = 0; i < args->nitems; i++) {
		tok.tok_t *x = eval(args->items[i]);
		if (x->type != tok.NUMBER) {
			panic("not a number");
		}
		double next = atof(x->value);
		switch (op) {
			case 1: { r *= next; }
			case 2: { r += next; }
			default: { panic("unknown op"); }
		}
	}
	return r;
}

// (* a b) returns a * b
tok.tok_t *mul(tok.tok_t *args) {
	char buf[100];
	printnum(buf, reduce(args, 1, 1));
	return tok.newnumber(buf);
}

// (+ a b) returns a + b
tok.tok_t *add(tok.tok_t *args) {
	char buf[100];
	printnum(buf, reduce(args, 0, 2));
	return tok.newnumber(buf);
}

// (- a b) returns a - b
tok.tok_t *sub(tok.tok_t *args) {
	char buf[100];

	tok.tok_t *a = eval(car(args));
	if (args->nitems == 1) {
		printnum(buf, -atof(a->value));
		return tok.newnumber(buf);
	}
	tok.tok_t *b = eval(car(cdr(args)));
	if (a->type != tok.NUMBER || b->type != tok.NUMBER) {
		panic("not a number");
	}
	printnum(buf, atof(a->value) - atof(b->value));
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
	printnum(buf, atof(a->value) / atof(b->value));
	return tok.newnumber(buf);
}

tok.tok_t *numeq(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));
	if (atof(a->value) == atof(b->value)) {
		return tok.newsym("true");
	}
	return NULL;
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

// (> a b) returns true if a > b
tok.tok_t *gt(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));
	if (atof(a->value) > atof(b->value)) {
		return tok.newsym("true");
	}
	return NULL;
}

// (< a b) returns true if a < b
tok.tok_t *lt(tok.tok_t *args) {
	tok.tok_t *a = eval(car(args));
	tok.tok_t *b = eval(car(cdr(args)));
	if (atof(a->value) < atof(b->value)) {
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
