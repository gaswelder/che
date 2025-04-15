#import rt.c

pub rt.item_t *eval(rt.item_t *i) {
	if (i->list) {
		return eval_list(i);
	}
	return i;
}

rt.item_t *eval_list(rt.item_t *l) {
	rt.item_t *first = rt.car(l);
	if (first->list) {
		dunno(first);
		return NULL;
	}
	return runfunc(first->data, rt.cdr(l));
}

rt.item_t *cond(rt.item_t *args) {
	rt.item_t *l = args;
	while (l) {
		rt.item_t *cas = rt.car(l);
		rt.item_t *cond = rt.car(cas);
		if (eval(cond)) {
			rt.item_t *result = rt.car(rt.cdr(cas));
			return eval(result);
		}
		l = rt.cdr(l);
	}
	return NULL;
}

rt.item_t *apply(rt.item_t *args) {
	rt.item_t *fn = rt.car(args);
	if (fn->list) {
		puts("how to apply list?");
		exit(1);
	}
	return runfunc(fn->data, eval(rt.car(rt.cdr(args))));
}

rt.item_t *runfunc(char *name, rt.item_t *args) {
	if (name == rt.intern("apply")) return apply(args);
	if (name == rt.intern("quote")) return rt.car(args);
	if (name == rt.intern("cons")) return cons(args);
	if (name == rt.intern("cond")) return cond(args);
	if (name == rt.intern("eq?")) return eq(args);
	panic("unknown function %s", name);
	return NULL;
}

rt.item_t *cons(rt.item_t *args) {
	return rt.cons(rt.car(args), rt.car(rt.cdr(args)));
}

// Implements the eq? function.
rt.item_t *eq(rt.item_t *args) {
	rt.item_t *a = eval(rt.car(args));
	rt.item_t *b = eval(rt.car(rt.cdr(args)));

	int t = rt.typeof(a);
	// If types don't match, then not equal.
	if (rt.typeof(b) != t) {
		return NULL;
	}
	switch (t) {
		// Compare symbols by their content.
		case rt.SYMBOL: {
			if (a->data == b->data) {
				return rt.sym("true");
			}
			return NULL;
		}
	}
	panic("unhandled item type: %d", t);
}

void dunno(rt.item_t *i) {
	fprintf(stderr, "how to eval? ");
	rt.dbgprint(i);
	exit(1);
}
