pub typedef {
    char *output;
    size_t output_cap;
    size_t output_size;
    bool _done;
    const char *_err;
} t;

pub t *make(char *buf, size_t n) {
    t *r = calloc(1, sizeof(t));
    r->output = buf;
    r->output_cap = n;
    return r;
}

pub void free(t *r) {
    free(r);
}

pub bool consume(t *r, char c) {
    if (r->_done || r->_err) {
        return false;
    }
    if (r->output_size + 2 >= r->output_cap) {
        r->_err = "buffer too small";
        return false;
    }
    // Always write the trailing zero in case the caller wants partial output.
    r->output[r->output_size++] = c;
    r->output[r->output_size] = '\0';
    if (c == '\n') {
        r->_done = true;
    }
    return true;
}

pub void reset(t *r) {
    r->output_size = 0;
    r->_done = false;
    r->_err = NULL;
}

pub bool done(t *r) {
    return r->_done;
}

pub const char *err(t *r) {
    return r->_err;
}
