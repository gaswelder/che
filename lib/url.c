#import parsebuf

pub typedef {
    char schema[10];
    char hostname[100];
    char port[10];
    char path[100];
} t;

pub t *parse(const char *s) {
	t *r = calloc(1, sizeof(t));
	if (!r) {
		panic("calloc failed");
	}

    parsebuf.parsebuf_t *buf = parsebuf.buf_new(s);

    // http
    char *q = r->schema;
    while (parsebuf.buf_more(buf) && parsebuf.buf_peek(buf) != ':') {
        *q++ = parsebuf.buf_get(buf);
    }

    // ://
    if (parsebuf.buf_get(buf) != ':' || parsebuf.buf_get(buf) != '/' || parsebuf.buf_get(buf) != '/') {
        parsebuf.buf_free(buf);
		free(r);
        return NULL;
    }

    // domain or ip address
    q = r->hostname;
    while (parsebuf.buf_more(buf) && parsebuf.buf_peek(buf) != ':' && parsebuf.buf_peek(buf) != '/') {
        *q++ = parsebuf.buf_get(buf);
    }

    q = r->port;
    if (parsebuf.buf_peek(buf) == ':') {
        parsebuf.buf_get(buf);
        while (parsebuf.buf_more(buf) && parsebuf.buf_peek(buf) != '/') {
            *q++ = parsebuf.buf_get(buf);
        }
    }

    q = r->path;
	*q = '/';
    if (parsebuf.buf_peek(buf) == '/') {
        while (parsebuf.buf_more(buf)) {
            *q++ = parsebuf.buf_get(buf);
        }
    }

    parsebuf.buf_free(buf);
    return r;
}
