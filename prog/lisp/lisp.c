#import read.c
#import rt.c
#import parsebuf

int main() {
	parsebuf.parsebuf_t *b = parsebuf.from_stdin();
	rt.item_t *input = read.read(b);
	rt.item_t *r = eval(input);

	char buf[4096];
	rt.print(r, buf, 4096);
	puts(buf);
	return 0;
}

void dunno(rt.item_t *i) {
	fprintf(stderr, "how to eval? ");
	char buf[4096];
	rt.print(i, buf, 4096);
	puts(buf);
	exit(1);
}

rt.item_t *eval(rt.item_t *i) {
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

rt.item_t *runfunc(char *name, rt.item_t *args) {
	if (name == rt.intern("apply")) return apply(args);
	if (name == rt.intern("quote")) return rt.car(args);
	if (name == rt.intern("cons")) return rt.cons(rt.car(args), rt.car(rt.cdr(args)));
	if (name == rt.intern("cond")) return cond(args);
	if (name == rt.intern("eq?")) {
		if (eval(rt.car(args)) == eval(rt.car(rt.cdr(args)))) {
			return rt.sym("true");
		}
		return NULL;
	}
	panic("unknown function %s", name);
	return NULL;
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
