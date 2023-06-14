#import strings

#define ALLOCITEMS_SIZE 8192

typedef {
    void *p;
    char *label;
} allocitem;

allocitem allocitems[ALLOCITEMS_SIZE] = {};

pub void lk_alloc_init() {
    memset(allocitems, 0, sizeof(allocitems));
}

// Add p to end of allocitems[].
pub void add_p(void *p, char *label) {
    int i;
    for (i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p == NULL) {
            allocitems[i].p = p;
            allocitems[i].label = label;
            break;
        }
    }
    assert(i < ALLOCITEMS_SIZE);
}

// Replace matching allocitems[] p with newp.
pub void replace_p(void *p, void *newp, char *label) {
    int i;
    for (i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p ==  p) {
            allocitems[i].p = newp;
            allocitems[i].label = label;
            break;
        }
    }
    assert(i < ALLOCITEMS_SIZE);
}

// Clear matching allocitems[] p.
pub void clear_p(void *p) {
    int i;
    for (i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p == p) {
            allocitems[i].p = NULL;
            allocitems[i].label = NULL;
            break;
        }
    }
    if (i >= ALLOCITEMS_SIZE) {
        printf("clear_p i: %d\n", i);
    }
    assert(i < ALLOCITEMS_SIZE);
}

pub void *lk_malloc(size_t size, char *label) {
    void *p = malloc(size);
    add_p(p, label);
    return p;
}

pub void *lk_realloc(void *p, size_t size, char *label) {
    void *newp = realloc(p, size);
    replace_p(p, newp, label);
    return newp;
}

pub void lk_free(void *p) {
    clear_p(p);
    free(p);
}

pub char *lk_strdup(const char *s, char *label) {
    char *sdup = strings.newstr("%s", s);
    add_p(sdup, label);
    return sdup;
}

pub void lk_print_allocitems() {
    printf("allocitems[] labels:\n");
    for (int i=0; i < ALLOCITEMS_SIZE; i++) {
        if (allocitems[i].p != NULL) {
            printf("%s\n", allocitems[i].label);
        }
    }
}
