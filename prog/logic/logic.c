#import parsebuf
#import map

enum { FUNC, VAR, CONST };

typedef {
    int type;
    char name[10];
    term *args[10];
    int args_length;
} term;

void print_term(term *t) {
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

map.map_t *unify(term *x, *y, map.map_t *subst) {
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
        if (!streq(x->name, y->name) || x->args_length != y->args_length) {
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

// Unifies variable v with term x, using subst.
map.map_t *unify_variable(term *v, term *x, map.map_t *subst) {
    if (map.map_has(subst, v->name)) {
        return unify(map.map_get(subst, v->name), x, subst);
    }
    if (x->type == VAR && map.map_has(subst, x->name)) {
        return unify(v, map.map_get(subst, x->name), subst);
    }
    if (term_has_var(v, x, subst)) {
        return NULL;
    }
    // v is not yet in subst and can't simplify x. Extend subst.
    map.map_set(subst, v->name, x);
    return subst;
}

// Does the variable v occur anywhere inside term?
bool term_has_var(term *t, *v, map.map_t *subst) {
    if (eq(v, t)) {
        return true;
    }
    if (t->type == VAR && map.map_has(subst, t->name)) {
        return term_has_var(map.map_get(subst, t->name), v, subst);
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

bool eq(term *x, *y) {
    if (x->type != y->type) {
        return false;
    }
    if (!streq(x->name, y->name)) {
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

term *parse_term1(parsebuf.parsebuf_t *b) {
    term *t = calloc(1, sizeof(term));

    int n = 0;
    while (isalpha(parsebuf.buf_peek(b))) {
        t->name[n++] = parsebuf.buf_get(b);
    }

    if (allupper(t->name)) {
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

term *parse_term(char *s) {
    return parse_term1(parsebuf.buf_new(s));
}

void print_map(map.map_t *m) {
    map.map_iter_t *it = map.map_iter(m);
    bool first = true;
    while (map.map_iter_next(it)) {
        if (first) {
            first = false;
        } else {
            fprintf(stdout, " ");
        }
        fprintf(stdout, "%s=", map.map_iter_key(it));
        print_term(map.map_iter_val(it));
    }
    map.map_iter_free(it);
}

void run(char *t1, *t2) {
    map.map_t *s = unify(parse_term(t1), parse_term(t2), map.map_new());
    if (s) {
        print_map(s);
        printf("\n");
    } else {
        printf("NULL\n");
    }
}

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

bool streq(char *a, char *b) {
	return strcmp(a, b) == 0;
}

bool allupper(char *s) {
    for (char *c = s; *c; c++) {
        if (islower(*c)) {
            return false;
        }
    }
    return true;
}