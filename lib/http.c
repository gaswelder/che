#import parsebuf
#import strings
#import strbuilder

pub enum {
	UNKNOWN_METHOD,
	GET,
	POST,
	HEAD
};

/*
 * Given a method name string returns the method's enum value.
 * Returns UNKNOWN_METHOD if the string doesn't match any method.
 */
pub int method_from_string(const char *s) {
	if (strings.casecmp(s, "GET")) return GET;
    if (strings.casecmp(s, "POST")) return POST;
    if (strings.casecmp(s, "HEAD")) return HEAD;
	return UNKNOWN_METHOD;
}

pub typedef {
    char name[1024];
    char value[1024];
} header_t;

pub typedef {
    char schema[10];
    char hostname[100];
    char port[10];
    char path[100];
} url_t;

pub bool parse_url(url_t *r, const char *s) {
    memset(r, 0, sizeof(url_t));

    parsebuf.parsebuf_t *buf = parsebuf.buf_new(s);
    
    // http
    char *q = r->schema;
    while (parsebuf.buf_more(buf) && parsebuf.buf_peek(buf) != ':') {
        *q++ = parsebuf.buf_get(buf);
    }

    // ://
    if (parsebuf.buf_get(buf) != ':'
        || parsebuf.buf_get(buf) != '/'
        || parsebuf.buf_get(buf) != '/'
        ) {
        parsebuf.buf_free(buf);
        return false;
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
    return true;
}



pub typedef {
	int err;
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

const char *errors[] = {
	"no error",
	"unknown method"
};

pub const char *errstr(int err) {
	if (err >= 0 && (size_t) err < nelem(errors)) {
		return errors[err];
	}
	return "unknown error";
}

pub bool init_request(request_t *r, int method, const char *path) {
	memset(r, 0, sizeof(request_t));
	const char *methodstring = NULL;
	switch (method) {
        case GET: { methodstring = "GET"; }
        case POST: { methodstring = "POST"; }
        case HEAD: { methodstring = "HEAD"; }
		default: {
			r->err = 1; // unknown method
			return false;
		}
    }
    strcpy(r->method, methodstring);
    strcpy(r->uri, path);
    strcpy(r->version, "HTTP/1.1");
    return true;
}

pub bool set_header(request_t *r, const char *name, *value) {
    header_t *h = &r->headers[r->nheaders++];
    strcpy(h->name, name);
    strcpy(h->value, value);
    return true;
}

pub bool write_request(request_t *r, char *buf, size_t n) {
    strbuilder.str *sb = strbuilder.str_new();
    bool ok = sb != NULL;
    
    ok = ok && strbuilder.adds(sb, r->method)
        && strbuilder.adds(sb, " ")
        && strbuilder.adds(sb, r->uri)
        && strbuilder.adds(sb, " ")
        && strbuilder.adds(sb, r->version)
        && strbuilder.adds(sb, "\r\n");
    for (size_t i = 0; i < r->nheaders; i++) {
        header_t *h = &r->headers[i];
        ok = ok
            && strbuilder.adds(sb, h->name)
            && strbuilder.adds(sb, ": ")
            && strbuilder.adds(sb, h->value)
            && strbuilder.adds(sb, "\r\n");
    }
    ok = ok && strbuilder.adds(sb, "\r\n");
    if (!ok || strbuilder.str_len(sb) > n) {
        strbuilder.str_free(sb);
        return false;
    }
    memcpy(buf, strbuilder.str_raw(sb), n);
    strbuilder.str_free(sb);
    return true;
}


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
    memset(r, 0, sizeof(request_t));
    char *lines[100] = {0};
    size_t nlines = strings.split("\r\n", head, lines, sizeof(lines));
    if (nlines == sizeof(lines)) {
        // Lines array too small.
        return false;
    }
    if (!parse_start_line(lines[0], r)) {
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
bool parse_start_line(char *line, request_t *r) {
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

pub typedef {
    char version[100];
    int status;
    char status_text[20];

    char servername[1000];
    int head_length;
    int content_length;
} response_t;

pub bool parse_response(char *data, response_t *r) {
    char *s = strstr(data, "\r\n\r\n");
    if (!s) {
        return false;
    }
    r->head_length = s - data + 4;

    // HTTP/1.1
    char *p = strstr(data, "HTTP/");
    if (!p) {
        return false;
    }
    strncpy(r->version, p, strlen("HTTP/1.x"));
    p += strlen("HTTP/1.x");

    // space
    p++;

    // 200
    char respcode[4] = {0};
    strncpy(respcode, p, 3);
    p += 3;
    sscanf(respcode, "%d", &r->status);


    // ...
    p = strstr(data, "Server:");
    if (p) {
        p += 8;
        char *q = r->servername;
        while (*p > 32) {
            *q++ = *p++;
        }
        *q = 0;
    }

    p = strstr(data, "Content-Length:");
    if (p) {
        r->content_length = atoi(p + 16);
    }

    return true;
}