#define N_GROW_REFLIST 10

LKRefList *lk_reflist_new() {
    LKRefList *l = lk_malloc(sizeof(LKRefList), "lk_reflist_new");
    l->items_size = N_GROW_REFLIST;
    l->items_len = 0;
    l->items_cur = 0;

    l->items = lk_malloc(l->items_size * sizeof(void*), "lk_reflist_new_items");
    memset(l->items, 0, l->items_size * sizeof(void*));
    return l;
}

void lk_reflist_free(LKRefList *l) {
    assert(l->items != NULL);

    memset(l->items, 0, l->items_size * sizeof(void*));
    lk_free(l->items);
    l->items = NULL;
    lk_free(l);
}

void lk_reflist_append(LKRefList *l, void *p) {
    assert(l->items_len <= l->items_size);

    if (l->items_len == l->items_size) {
        void **pitems = lk_realloc(l->items, (l->items_size+N_GROW_REFLIST) * sizeof(void*), "lk_reflist_append");
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

void *lk_reflist_get(LKRefList *l, unsigned int i) {
    if (l->items_len == 0) return NULL;
    if (i >= l->items_len) return NULL;
    return l->items[i];
}

void *lk_reflist_get_cur(LKRefList *l) {
    if (l->items_cur >= l->items_len) {
        return NULL;
    }
    return l->items[l->items_cur];
}

void lk_reflist_remove(LKRefList *l, unsigned int i) {
    if (l->items_len == 0) return;
    if (i >= l->items_len) return;

    int num_items_after = l->items_len-i-1;
    memmove(l->items+i, l->items+i+1, num_items_after * sizeof(void*));
    memset(l->items+l->items_len-1, 0, sizeof(void*));
    l->items_len--;
}

void lk_reflist_clear(LKRefList *l) {
    memset(l->items, 0, l->items_size * sizeof(void*));
    l->items_len = 0;
    l->items_cur = 0;
}

