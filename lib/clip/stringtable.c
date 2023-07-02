#import strings

pub typedef {
    char *k;
    char *v;
} entry_t;

pub typedef {
    entry_t *items;
    size_t len;
    size_t cap;
} t;

pub t *new() {
    t *st = calloc(1, sizeof(t));
    st->cap = 8;
    st->len = 0;
    st->items = calloc(st->cap, sizeof(entry_t));
    return st;
}

pub void free(t *st) {
    for (size_t i = 0; i < st->len; i++) {
        OS.free(st->items[i].k);
        OS.free(st->items[i].v);
    }
    OS.free(st->items);
    OS.free(st);
}

pub void set(t *st, const char *ks, const char *v) {
    // If item already exists, overwrite it.
    for (size_t i = 0; i < st->len; i++) {
        if (!strcmp(st->items[i].k, ks)) {
            OS.free(st->items[i].v);
            st->items[i].v = strings.newstr("%s", v);
            return;
        }
    }

    // If reached capacity, expand the array and add new item.
    if (st->len == st->cap) {
        st->cap += 10;
        st->items = realloc(st->items, st->cap * sizeof(entry_t));
        memset(st->items + st->len, 0, (st->cap - st->len) * sizeof(entry_t));
    }

    st->items[st->len].k = strings.newstr("%s", ks);
    st->items[st->len].v = strings.newstr("%s", v);
    st->len++;
}

pub char *get(t *st, const char *ks) {
    for (size_t i = 0; i < st->len; i++) {
        if (!strcmp(st->items[i].k, ks)) {
            return st->items[i].v;
        }
    }
    return NULL;
}
