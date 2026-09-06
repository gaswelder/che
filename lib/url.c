#import scanner

pub typedef {
    char schema[10];
    char hostname[100];
    char port[10];
    char path[100];
} t;

pub t *parse(const char *s) {
	t *r = calloc!(1, sizeof(t));
    scanner.t *buf = scanner.from_str(s);

    // http
    char *q = r->schema;
    while (scanner.more(buf) && scanner.peek(buf) != ':') {
        *q++ = scanner.get(buf);
    }

    // ://
    if (scanner.get(buf) != ':' || scanner.get(buf) != '/' || scanner.get(buf) != '/') {
        scanner.free(buf);
		free(r);
        return NULL;
    }

    // domain or ip address
    q = r->hostname;
    while (scanner.more(buf) && scanner.peek(buf) != ':' && scanner.peek(buf) != '/') {
        *q++ = scanner.get(buf);
    }

    q = r->port;
    if (scanner.peek(buf) == ':') {
        scanner.get(buf);
        while (scanner.more(buf) && scanner.peek(buf) != '/') {
            *q++ = scanner.get(buf);
        }
    }

    q = r->path;
	*q = '/';
    if (scanner.peek(buf) == '/') {
        while (scanner.more(buf)) {
            *q++ = scanner.get(buf);
        }
    }

    scanner.free(buf);
    return r;
}
