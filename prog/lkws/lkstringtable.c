#import lkstring.c

pub typedef {
    lkstring.LKString *k;
    lkstring.LKString *v;
} LKStringTableItem;

pub typedef {
    LKStringTableItem *items;
    size_t items_len;
    size_t items_size;
} LKStringTable;

pub LKStringTable *lk_stringtable_new() {
    LKStringTable *st = calloc(1, sizeof(LKStringTable));
    st->items_size = 1; // start with room for n items
    st->items_len = 0;
    st->items = calloc(st->items_size, sizeof(LKStringTableItem));
    return st;
}

pub void lk_stringtable_free(LKStringTable *st) {
    assert(st->items != NULL);

    for (size_t i = 0; i < st->items_len; i++) {
        lkstring.lk_string_free(st->items[i].k);
        lkstring.lk_string_free(st->items[i].v);
    }
    memset(st->items, 0, st->items_size * sizeof(LKStringTableItem));

    free(st->items);
    st->items = NULL;
    free(st);
}

pub void lk_stringtable_set(LKStringTable *st, const char *ks, const char *v) {
    assert(st->items_size >= st->items_len);

    // If item already exists, overwrite it.
    for (size_t i = 0; i < st->items_len; i++) {
        if (lkstring.lk_string_sz_equal(st->items[i].k, ks)) {
            lkstring.lk_string_assign(st->items[i].v, v);
            return;
        }
    }

    // If reached capacity, expand the array and add new item.
    if (st->items_len == st->items_size) {
        st->items_size += 10;
        st->items = realloc(st->items, st->items_size * sizeof(LKStringTableItem));
        memset(st->items + st->items_len, 0,
               (st->items_size - st->items_len) * sizeof(LKStringTableItem));
    }

    st->items[st->items_len].k = lkstring.lk_string_new(ks);
    st->items[st->items_len].v = lkstring.lk_string_new(v);
    st->items_len++;
}

pub char *lk_stringtable_get(LKStringTable *st, const char *ks) {
    for (size_t i = 0; i < st->items_len; i++) {
        if (lkstring.lk_string_sz_equal(st->items[i].k, ks)) {
            return st->items[i].v->s;
        }
    }
    return NULL;
}
