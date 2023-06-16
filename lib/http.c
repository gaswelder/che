#import parsebuf

pub typedef {
    char method[10];
    char path[1024];
    char version[10];
} request_line_t;

pub bool parse_request_line(const char *line, request_line_t *r) {
    parsebuf.parsebuf_t *b = parsebuf.buf_new(line);

    // method
    char *method = r->method;
    while (parsebuf.buf_more(b) && !isspace(parsebuf.buf_peek(b))) {
        *method = parsebuf.buf_get(b);
        method++;
    }
    while (isspace(parsebuf.buf_peek(b))) parsebuf.buf_get(b);

    // path
    char *path = r->path;
    while (parsebuf.buf_more(b) && !isspace(parsebuf.buf_peek(b))) {
        *path = parsebuf.buf_get(b);
        path++;
    }
    while (isspace(parsebuf.buf_peek(b))) parsebuf.buf_get(b);

    // version
    char *version = r->version;
    while (parsebuf.buf_more(b) && !isspace(parsebuf.buf_peek(b))) {
        *version = parsebuf.buf_get(b);
        version++;
    }
    while (isspace(parsebuf.buf_peek(b))) parsebuf.buf_get(b);

    bool ok = !parsebuf.buf_more(b);
    parsebuf.buf_free(b);
    return ok;
}
