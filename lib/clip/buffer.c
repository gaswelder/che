/**
 * This is the buffer handle.
 * Its only purpose is to be an opaque container for a queue of "clips".
 */
pub typedef {
    void *first;
    void *last;
} t;

/**
 * A "clip" is a dynamically allocated frame of bytes.
 * Generally, writing n bytes to the buffer results in allocation of a new clip
 * with n bytes inside.
 */
pub typedef {
    // The data that was written.
    char *data;

    // The size of the data.
    size_t size;

    // How many bytes of the data have already been consumed by reading.
    size_t pos;

    // Next clip.
    clip_t *next;
} clip_t;

pub t *new() {
    t *b = calloc(1, sizeof(t));
    return b;
}

pub void free(t *b) {
    while (true) {
        clip_t *c = b->first;
        if (!c) break;
        b->first = c->next;
        OS.free(c->data);
        OS.free(c);
    }
}

pub size_t size(t *b) {
    size_t total = 0;
    clip_t *c = b->first;
    while (c) {
        total += c->size - c->pos;
        c = c->next;
    }
    return total;
}

/**
 * Places len bytes from buf to the buffer.
 * Returns false on failure.
 */
pub bool write(t *b, char *buf, size_t len) {
    if (len == 0) return false;
    clip_t *c = calloc(1, sizeof(clip_t));
    if (!c) return false;
    c->data = calloc(len, 1);
    if (!c->data) {
        OS.free(c);
        return false;
    }
    memcpy(c->data, buf, len);
    c->size = len;

    // append to the end of the list.
    if (b->last) {
        clip_t *last = b->last;
        last->next = c;
        b->last = c;
    } else {
        b->first = c;
        b->last = c;
    }
	return true;
}

/**
 * Moves up to bufsize bytes to buf, removing them
 * from the buffer. Returns the number of moved bytes.
 */
pub size_t read(t *b, char *buf, size_t bufsize) {
    char *p = buf;
    for (size_t i = 0; i < bufsize; i++) {
        clip_t *c = b->first;
        if (!c) break;
        *p++ = c->data[c->pos++];

        // When the whole clip has been read, remove it.
        if (c->pos == c->size) {
            b->first = c->next;
            if (!c->next) {
                b->last = NULL;
            }
            OS.free(c->data);
            OS.free(c);
        }
    }
    return p - buf;
}
