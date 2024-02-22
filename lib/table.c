pub typedef {
    char left[1000];
    char right[1000];
} item_t;

pub typedef {
    item_t items[1000];
    size_t nitems;
} t;

pub void add(t *tbl, const char *title, const char *fmt, ...) {
    tbl->nitems++;
    item_t *it = &tbl->items[tbl->nitems];
    strcpy(it->left, title);
    
    va_list l = {0};
	va_start(l, fmt);
	vsprintf(it->right, fmt, l);
	va_end(l);
}

pub void split(t *tbl) {
    tbl->nitems++;
}

pub void print(t *tbl) {
    for (size_t i = 0; i < tbl->nitems; i++) {
        item_t *it = &tbl->items[i];
        if (!it->left[0]) {
            printf("\n");
            continue;
        }
        printf("%-30s%s\n", it->left, it->right);
    }
}
