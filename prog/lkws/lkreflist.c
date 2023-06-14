
#define N_GROW_REFLIST 10

pub typedef {
    void **items;
    size_t items_len;
    size_t items_size;
    size_t items_cur;
} LKRefList;

const int SIZEOF_VOIDP = 8; // sizeof(void *), TODO

pub LKRefList *lk_reflist_new() {
    LKRefList *l = calloc(1, sizeof(LKRefList));
    l->items_size = N_GROW_REFLIST;
    l->items_len = 0;
    l->items_cur = 0;
    l->items = calloc(1, l->items_size * SIZEOF_VOIDP);
    return l;
}

pub void lk_reflist_free(LKRefList *l) {
    assert(l->items != NULL);
    free(l->items);
    l->items = NULL;
    free(l);
}

pub void lk_reflist_append(LKRefList *l, void *p) {
    assert(l->items_len <= l->items_size);

    if (l->items_len == l->items_size) {
        void **pitems = realloc(l->items, (l->items_size+N_GROW_REFLIST) * SIZEOF_VOIDP);
        if (pitems == NULL) {
            return;
        }
        l->items = pitems;
        l->items_size += N_GROW_REFLIST;
    }
    l->items[l->items_len] = p;
    l->items_len++;

    assert(l->items_len <= l->items_size);
}

pub void *lk_reflist_get(LKRefList *l, size_t i) {
    if (l->items_len == 0) return NULL;
    if (i >= l->items_len) return NULL;
    return l->items[i];
}

pub void lk_reflist_remove(LKRefList *l, size_t i) {
    if (l->items_len == 0) return;
    if (i >= l->items_len) return;

    int num_items_after = l->items_len-i-1;
    memmove(l->items+i, l->items+i+1, num_items_after * SIZEOF_VOIDP);
    memset(l->items+l->items_len-1, 0, SIZEOF_VOIDP);
    l->items_len--;
}

pub void lk_reflist_clear(LKRefList *l) {
    memset(l->items, 0, l->items_size * SIZEOF_VOIDP);
    l->items_len = 0;
    l->items_cur = 0;
}
