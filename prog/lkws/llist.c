
pub typedef {
    void *data;
    t *_next;
} t;

pub t *new() {
    t *i = calloc(1, sizeof(t));
    return i;
}

pub void append(t *l, void *data) {
    if (!data) {
        return;
    }
    t *i = calloc(1, sizeof(t));
    if (!i) {
        panic("calloc");
    }
    i->data = data;

    t *p = l;
    while (p->_next) {
        p = p->_next;
    }
    p->_next = i;
}

pub void remove(t *l, void *data) {
    t *p = l;
    t *q = p->_next;
    while (q) {
        if (q->data == data) {
            p->_next = q->_next;
            free(q);
            return;
        }
        p = q;
        q = q->_next;
    }
}

pub void *next(t *l) {
    return l->_next;
}
