#import clip/map
#import parsebuf
#import strings

enum { FUNC, VAR, CONST };

typedef {
    int type;
    char name[10];
    term_t *args[10];
    int args_length;
} term_t;

// A hack until sizeof is fixed.
const size_t POINTERSIZE = 8;

int main() {
    run("a", "b");
    run("a", "X");
    run("f(a, b, bar(t))", "f(a, V, X)");
    // {'V': b, 'X': bar(t)}
    run("f(a, V, bar(D))", "f(D, k, bar(a))");
    // {'D': a, 'V': k}
    run("f(X, Y)", "f(Z, g(X))");
    // {'X': Z, 'Y': g(X)}
    run("f(X, Y, X)", "f(r, g(X), p)");
    return 0;
}

void run(const char *t1, *t2) {
	map.map_t *subst = map.new(POINTERSIZE);
    map.map_t *s = unify(parse_term(t1), parse_term(t2), subst);
    if (s) {
        print_map(s);
        printf("\n");
    } else {
        printf("NULL\n");
    }
}

map.map_t *unify(term_t *x, *y, map.map_t *subst) {
    if (eq(x, y)) {
        return subst;
    }
    if (x->type == VAR) {
        return unify_variable(x, y, subst);
    }
    if (y->type == VAR) {
        return unify_variable(y, x, subst);
    }
    if (x->type == FUNC && y->type == FUNC) {
        if (!strings.eq(x->name, y->name) || x->args_length != y->args_length) {
            return NULL;
        }
        map.map_t *r = subst;
        for (int i = 0; i < x->args_length; i++) {
            r = unify(x->args[i], y->args[i], r);
            if (!r) {
                break;
            }
        }
        return r;
    }
    return NULL;
}

// Unifies variable v with term_t x, using subst.
map.map_t *unify_variable(term_t *v, term_t *x, map.map_t *subst) {
	term_t *deref = NULL;
    if (map.gets(subst, v->name, &deref)) {
        return unify(deref, x, subst);
    }
    if (x->type == VAR && map.gets(subst, x->name, &deref)) {
        return unify(v, deref, subst);
    }
    if (term_has_var(v, x, subst)) {
        return NULL;
    }
    // v is not yet in subst and can't simplify x. Extend subst.
    map.sets(subst, v->name, &x);
    return subst;
}

void print_term(term_t *t) {
    switch (t->type) {
        case FUNC: {
            printf("%s(", t->name);
            for (int i = 0; i < t->args_length; i++) {
                if (i > 0) {
                    printf(", ");
                }
                print_term(t->args[i]);
            }
            printf(")");
        }
        case VAR: { printf("%s", t->name); }
        case CONST: { printf("%s", t->name); }
    }
}





// Does the variable v occur anywhere inside term_t?
bool term_has_var(term_t *t, *v, map.map_t *subst) {
    if (eq(v, t)) {
        return true;
    }
	term_t *deref = NULL;
    if (t->type == VAR && map.gets(subst, t->name, &deref)) {
        return term_has_var(deref, v, subst);
    }
    if (t->type == FUNC) {
        for (int i = 0; i < t->args_length; i++) {
            if (term_has_var(t->args[i], v, subst)) {
                return true;
            }
        }
        return false;
    }
    return false;
}

bool eq(term_t *x, *y) {
    if (x->type != y->type) {
        return false;
    }
    if (!strings.eq(x->name, y->name)) {
        return false;
    }
    if (x->type == FUNC) {
        for (int i = 0; i < x->args_length; i++) {
            if (!eq(x->args[i], y->args[i])) {
                return false;
            }
        }
    }
    return true;
}

term_t *parse_term1(parsebuf.parsebuf_t *b) {
    term_t *t = calloc(1, sizeof(term_t));

    int n = 0;
    while (isalpha(parsebuf.buf_peek(b))) {
        t->name[n++] = parsebuf.buf_get(b);
    }

    if (strings.allupper(t->name)) {
        t->type = VAR;
        return t;
    }

    if (parsebuf.buf_skip(b, '(')) {
        t->type = FUNC;
        while (true) {
            t->args[t->args_length++] = parse_term1(b);
            if (parsebuf.buf_skip(b, ',')) {
                parsebuf.buf_skip_set(b, " ");
                continue;
            }
            break;
        }
        if (!parsebuf.buf_skip(b, ')')) {
            panic("missing closing brace: '%c'", parsebuf.buf_peek(b));
        }
        return t;
    }
    t->type = CONST;
    return t;
}

term_t *parse_term(const char *s) {
    return parse_term1(parsebuf.buf_new(s));
}

void print_map(map.map_t *m) {
    bool first = true;
	uint8_t key[100];

	map.iter_t *it = map.iter(m);
    while (map.next(it)) {
        if (first) {
            first = false;
        } else {
            fprintf(stdout, " ");
        }
		key[map.itkey(it, key, sizeof(key))] = 0;
        fprintf(stdout, "%s=", key);
		term_t *x = NULL;
		map.itval(it, &x);
        print_term(x);
    }
    map.end(it);
}

