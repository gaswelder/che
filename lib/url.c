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

    parsebuf.parsebuf_t *buf = parsebuf.from_str(s);

    // http
    char *q = r->schema;
    while (parsebuf.more(buf) && parsebuf.peek(buf) != ':') {
        *q++ = parsebuf.get(buf);
    }

    // ://
    if (parsebuf.get(buf) != ':' || parsebuf.get(buf) != '/' || parsebuf.get(buf) != '/') {
        parsebuf.free(buf);
		free(r);
        return NULL;
    }

    // domain or ip address
    q = r->hostname;
    while (parsebuf.more(buf) && parsebuf.peek(buf) != ':' && parsebuf.peek(buf) != '/') {
        *q++ = parsebuf.get(buf);
    }

    q = r->port;
    if (parsebuf.peek(buf) == ':') {
        parsebuf.get(buf);
        while (parsebuf.more(buf) && parsebuf.peek(buf) != '/') {
            *q++ = parsebuf.get(buf);
        }
    }

    q = r->path;
	*q = '/';
    if (parsebuf.peek(buf) == '/') {
        while (parsebuf.more(buf)) {
            *q++ = parsebuf.get(buf);
        }
    }

    parsebuf.free(buf);
    return r;
}
