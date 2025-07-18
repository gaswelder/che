pub typedef {
    size_t cap;
    size_t len;
    size_t itemsize;
    void *data;
} t;

// Creates a new instance of vector.
// itemsize specifies the size of a single item in bytes.
// Panics if memory allocation fails.
pub t *new(size_t itemsize) {
    t *l = calloc!(1, sizeof(t));
    l->itemsize = itemsize;
    return l;
}

// Frees the memory used by the vec.
pub void free(t *l) {
    if (l->cap > 0) OS.free(l->data);
    OS.free(l);
}

// Creates a new entry at the end of the list and copies data from x.
pub void push(t *l, void *x) {
	void *pos = alloc(l);
    memcpy(pos, x, l->itemsize);
}

// Creates a new entry at the end and returns its address.
pub void *alloc(t *l) {
	// Grow as needed.
    if (l->len == l->cap) {
        size_t newcap = l->cap * 2;
        if (newcap == 0) newcap = 16;
        void *newdata = realloc(l->data, newcap * l->itemsize);
        if (!newdata) {
            panic("failed to allocate memory");
        }
        l->cap = newcap;
        l->data = newdata;
        memset(index(l, l->len), 0, l->itemsize * (l->cap - l->len));
    }
	return index(l, l->len++);
}

// Returns a pointer to i-th item in the vec.
pub void *index(t *l, size_t i) {
    char *x = l->data;
    x += l->itemsize * i;
    return x;
}

// Returns the length of the vec.
pub size_t len(t *l) {
    return l->len;
}
