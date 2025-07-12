
typedef {
    uint8_t *bytes;
    size_t size;
} slice_t;

bool slice_eq(slice_t a, b) {
    return a.size == b.size && memcmp(a.bytes, b.bytes, a.size) == 0;
}

uint8_t *clone(uint8_t *b, size_t n) {
    uint8_t *c = calloc!(n, 1);
    memcpy(c, b, n);
    return c;
}

const int DEBUG = false;

pub typedef { void *m; } map_t;

// Creates a new map.
pub void *new() {
    map_t *r = calloc!(1, sizeof(map_t));
    // Start with 1 bucket, equivalent to a linear search array.
    r->m = innermap(1, 32);
    return r;
}

// Releases memory used by the map.
pub void free(map_t *m) {
    inner_free(m->m);
    OS.free(m);
}

pub void set(map_t *m, uint8_t *key, size_t keysize, void *val, size_t valsize) {
    // Enlarge the map if needed.
    innermap_t *cur = m->m;
    if (cur->size * 100 / cap(cur) > 75) {
        innermap_t *n = innermap(cur->nbuckets * 2, cur->bucketsize);
        inner_copy(n, cur); // m to n
        if (DEBUG) {
            inner_inspect(cur);
            inner_inspect(n);
        }
        inner_free(cur);
        m->m = n;
    }
    slice_t k = { .bytes = key, .size = keysize };
    slice_t v = { .bytes = val, .size = valsize };
    inner_set(m->m, k, v);
}

// Returns a pointer to the value stored at key or null.
// The caller must not keep the pointer because it will be invalidated on map resize.
pub void *get(map_t *m, uint8_t *key, size_t keysize) {
    slice_t k = { .bytes = key, .size = keysize };
    return inner_get(m->m, k);
}

typedef {
    size_t nbuckets, bucketsize;
    slot_t *slots;
    size_t size;
} innermap_t;

typedef {
    size_t bucket_index;
    uint32_t hash;
    slice_t key, val;
} slot_t;

size_t cap(innermap_t *m) {
    return m->nbuckets * m->bucketsize;
}

innermap_t *innermap(size_t nbuckets, bucketsize) {
    innermap_t *m = calloc!(1, sizeof(innermap_t));
    m->nbuckets = nbuckets;
    m->bucketsize = bucketsize;
    m->slots = calloc!(nbuckets * bucketsize, sizeof(slot_t));
    return m;
}

void inner_free(innermap_t *m) {
    size_t n = cap(m);
    for (size_t i = 0; i < n; i++) {
        slot_t *s = &m->slots[i];
        if (s->key.bytes != NULL) {
            OS.free(s->key.bytes);
            OS.free(s->val.bytes);
        }
    }
    OS.free(m->slots);
    OS.free(m);
}

void inner_set(innermap_t *m, slice_t key, val) {
    slot_t *s = find_slot(m, key);
    if (s != NULL) {
        OS.free(s->val.bytes);
        s->val.bytes = clone(val.bytes, val.size);
        return;
    }
    s = alloc_slot(m, key);
    m->size++;

    s->key.bytes = clone(key.bytes, key.size);
    s->key.size = key.size;

    s->val.bytes = clone(val.bytes, val.size);
    s->val.size = val.size;
}

void *inner_get(innermap_t *m, slice_t key) {
    slot_t *s = find_slot(m, key);
    if (s == NULL) {
        return NULL;
    }
    return s->val.bytes;
}

void inner_copy(innermap_t *dest, *src) {
    size_t n = cap(src);
    if (cap(dest) < n) {
        panic("dest is too small");
    }
    for (size_t i = 0; i < n; i++) {
        slot_t *s = &src->slots[i];
        if (s->key.bytes == NULL) {
            continue;
        }
        inner_set(dest, s->key, s->val);
    }
}

void inner_inspect(innermap_t *m) {
    printf("-----------\n");
    for (size_t b = 0; b < m->nbuckets; b++) {
        printf("[%zu]:", b);
        for (size_t i = 0; i < m->bucketsize; i++) {
            slot_t *s = &m->slots[b * m->bucketsize + i];
            if (s->key.bytes == NULL) {
                printf("\t. . .");
            } else {
                printf("\t%s (%zu)", (char *)s->key.bytes, s->bucket_index);
            }
        }
        printf("\n");
    }
    printf("-----------\n");
}

slot_t *find_slot(innermap_t *m, slice_t key) {
    uint32_t h = hash(key.bytes, key.size);
    size_t pos = (h % m->nbuckets) * m->bucketsize;
    size_t n = cap(m);

    for (size_t i = 0; i < n; i++) {
        slot_t *s = &m->slots[pos];
        if (s->hash == h && slice_eq(s->key, key)) {
            return s;
        }
        pos = (pos + 1) % n;
    }
    return NULL;
}

slot_t *alloc_slot(innermap_t *m, slice_t key) {
    uint32_t h = hash(key.bytes, key.size);
    size_t b = h % m->nbuckets;
    size_t pos = b * m->bucketsize;
    size_t n = cap(m);

    for (size_t i = 0; i < n; i++) {
        slot_t *s = &m->slots[pos];
        if (s->key.bytes == NULL) {
            s->hash = h;
            s->bucket_index = b;
            return s;
        }
        pos = (pos + 1) % n;
    }
    panic("failed to find an empty slot");
}

uint32_t hash(uint8_t *s, size_t n) {
    uint32_t r = 0;
    for (size_t i = 0; i < n; i++) {
        uint8_t b = s[i];
        r = (r * 31 + b);
    }
    return r;
}
