#import parsebuf
#import strings

pub typedef {
    char name[1024];
    char value[1024];
} header_t;

pub typedef {
    char method[10];
    char uri[1024];
    char version[10];

    // parsed uri
    char path[1024];
    char filename[1024];
    char query[1024];

    // Headers
    header_t headers[100];
    size_t nheaders;
} request_t;

pub header_t *get_header(request_t *r, const char *name) {
    header_t *h = NULL;
    for (size_t i = 0; i < r->nheaders; i++) {
        h = &r->headers[i];
        if (!strcmp(name, h->name)) {
            return h;
        }
    }
    return NULL;
}

pub bool parse_request(request_t *r, const char *head) {
    char *lines[100] = {0};
    size_t nlines = strings.split("\r\n", head, lines, sizeof(lines));
    if (nlines == sizeof(lines)) {
        // Lines array too small.
        return false;
    }
    if (!parse_request_line(lines[0], r)) {
        return false;
    }
    for (size_t i = 1; i < nlines; i++) {
        if (!strcmp(lines[i], "")) {
            break;
        }
        if (!parse_header_line(lines[i], &r->headers[r->nheaders])) {
            return false;
        }
        r->nheaders++;
    }
    for (size_t i = 0; i < nlines; i++) {
        free(lines[i]);
    }
    return true;
}

/**
 * Parses an HTTP request line in format "GET /path/blog/file1.html?a=1&b=2 HTTP/1.0".
 * Puts the values into the provided struct r.
 * Returns false on failure.
 */
bool parse_request_line(char *line, request_t *r) {
    parsebuf.parsebuf_t *b = parsebuf.buf_new(line);

    // method
    char *method = r->method;
    while (parsebuf.buf_more(b) && !isspace(parsebuf.buf_peek(b))) {
        *method = parsebuf.buf_get(b);
        method++;
    }
    while (isspace(parsebuf.buf_peek(b))) parsebuf.buf_get(b);

    // uri
    char *uri = r->uri;
    while (parsebuf.buf_more(b) && !isspace(parsebuf.buf_peek(b))) {
        *uri = parsebuf.buf_get(b);
        uri++;
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
    if (!ok) {
        return false;
    }

    return parse_query(r);
}

bool parse_query(request_t *r) {
    // Read full path
    char *pathp = r->path;
    const char *p = r->uri;
    while (*p && *p != '?') {
        *pathp++ = *p++;
    }
    // Read query string
    if (*p == '?') {
        p++;
        char *qsp = r->query;
        while (*p) {
            *qsp++ = *p++;
        }
    }

    // Remove trailing slashes from path
    strings.rtrim(r->path, "/");

    // Extract filename from path. "/path/blog/file1.html" ==> "file1.html"
    const char *filename = strrchr(r->path, '/');
    if (*filename == '/') {
        filename++;
    }
    strcpy(r->filename, filename);
    return true;
}

bool parse_header_line(const char *line, header_t *h) {
    const char *p = line;

    char *n = h->name;
    while (*p && *p != ':') {
        *n++ = *p++;
    }
    *n = '\0';

    if (*p != ':') {
        return false;
    }
    p++;
    while (*p && isspace(*p)) p++;

    char *v = h->value;
    while (*p) {
        *v++ = *p++;
    }
    return true;
}
