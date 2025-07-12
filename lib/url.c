#import tokenizer

pub typedef {
    char schema[10];
    char hostname[100];
    char port[10];
    char path[100];
} t;

pub t *parse(const char *s) {
	t *r = calloc!(1, sizeof(t));
    tokenizer.t *buf = tokenizer.from_str(s);

    // http
    char *q = r->schema;
    while (tokenizer.more(buf) && tokenizer.peek(buf) != ':') {
        *q++ = tokenizer.get(buf);
    }

    // ://
    if (tokenizer.get(buf) != ':' || tokenizer.get(buf) != '/' || tokenizer.get(buf) != '/') {
        tokenizer.free(buf);
		free(r);
        return NULL;
    }

    // domain or ip address
    q = r->hostname;
    while (tokenizer.more(buf) && tokenizer.peek(buf) != ':' && tokenizer.peek(buf) != '/') {
        *q++ = tokenizer.get(buf);
    }

    q = r->port;
    if (tokenizer.peek(buf) == ':') {
        tokenizer.get(buf);
        while (tokenizer.more(buf) && tokenizer.peek(buf) != '/') {
            *q++ = tokenizer.get(buf);
        }
    }

    q = r->path;
	*q = '/';
    if (tokenizer.peek(buf) == '/') {
        while (tokenizer.more(buf)) {
            *q++ = tokenizer.get(buf);
        }
    }

    tokenizer.free(buf);
    return r;
}
