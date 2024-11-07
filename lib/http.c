#import enc/urlencode
#import parsebuf
#import reader
#import writer
#import strbuilder
#import strings

pub enum {
	UNKNOWN_METHOD = 0,
	GET = 1,
	POST = 2,
	HEAD
};

/*
 * Given a method name string returns the method's enum value.
 * Returns UNKNOWN_METHOD (=0) if the string doesn't match any method.
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
	char name[1024];
    char value[1024];
	size_t valuelen;
} kv_t;

pub typedef {
	char version[10]; // "HTTP/1.0"
	char method[10]; // GET
	char path[1024]; // /img/index.html
	kv_t params[100]; // ?a=123&b=foo
	size_t nparams;
	header_t headers[100]; // Accept: */*
    size_t nheaders;

	// ...
	int err;
    char uri[1024];
    char filename[1024];
    char query[1024];
} request_t;

pub typedef {
	char version[10]; // "HTTP/1.0"
	int status; // 200
	header_t headers[100]; // Content-Type: text/html
    size_t nheaders;

	// ...
	char body[1000];
    int content_length;
} response_t;

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

pub request_t *newreq(int method, const char *path) {
	request_t *r = calloc(1, sizeof(request_t));
	if (!r) return NULL;
	if (!init_request(r, method, path)) {
		panic("init_request failed");
	}
	return r;
}

pub void freereq(request_t *r) {
	free(r);
}

pub void reqparams(request_t *r, const char *name, *value) {
	reqparam(r, name, value, strlen(value));
}

pub void reqparam(request_t *r, const char *name, const char *data, size_t datalen) {
	kv_t *h = &r->params[r->nparams++];
	strcpy(h->name, name);
	memcpy(h->value, data, datalen);
	h->valuelen = datalen;
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
    strcpy(r->version, "HTTP/1.0");
    return true;
}

pub bool set_header(request_t *r, const char *name, *value) {
    header_t *h = &r->headers[r->nheaders++];
    strcpy(h->name, name);
    strcpy(h->value, value);
    return true;
}

// Writes the request to the writer w.
// Return the number of bytes written or -1 on error.
pub int write_request(writer.t *w, request_t *r) {
    strbuilder.str *sb = strbuilder.str_new();
    bool ok = sb != NULL;

	// Request line.
    ok = ok && strbuilder.adds(sb, r->method)
		&& strbuilder.adds(sb, " ")
		&& strbuilder.adds(sb, r->uri);

	// Query params.
	for (size_t i = 0; i < r->nparams; i++) {
		if (i == 0) strbuilder.adds(sb, "?");
		else strbuilder.adds(sb, "&");

		kv_t *param = &r->params[i];
		strbuilder.adds(sb, param->name);
		strbuilder.adds(sb, "=");

		uint8_t tmp[1000] = {};
		writer.t *w2 = writer.static_buffer(tmp, sizeof(tmp));
		urlencode.write(w2, param->value, param->valuelen);
		writer.free(w2);

		strbuilder.adds(sb, (char *)tmp);
	}

	ok = ok
        && strbuilder.adds(sb, " ")
        && strbuilder.adds(sb, r->version)
        && strbuilder.adds(sb, "\r\n");

	// Headers.
    for (size_t i = 0; i < r->nheaders; i++) {
        header_t *h = &r->headers[i];
        ok = ok
            && strbuilder.adds(sb, h->name)
            && strbuilder.adds(sb, ": ")
            && strbuilder.adds(sb, h->value)
            && strbuilder.adds(sb, "\r\n");
    }

    ok = ok && strbuilder.adds(sb, "\r\n");

	const char *s = strbuilder.str_raw(sb);
	int len = writer.write(w, (uint8_t *)s, strlen(s));
	ok = ok && len == (int)strlen(s);
    strbuilder.str_free(sb);
    if (!ok) return -1;
	return len;
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

pub const char *get_res_header(response_t *r, const char *name) {
	header_t *h = NULL;
    for (size_t i = 0; i < r->nheaders; i++) {
        h = &r->headers[i];
        if (!strcmp(name, h->name)) {
            return h->value;
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

pub bool parse_response(reader.t *re, response_t *r) {
	parsebuf.parsebuf_t *b = parsebuf.new(re);
	if (!read_status_line(b, r)) {
		parsebuf.buf_free(b);
		return false;
	}
	while (parsebuf.buf_more(b)) {
		if (parsebuf.buf_skip_literal(b, "\r\n")) {
			break;
		}
		header_t *h = &r->headers[r->nheaders++];
		if (!read_header(b, h)) {
			parsebuf.buf_free(b);
			return false;
		}
	}
	bool ok = read_body(b, r);
	parsebuf.buf_free(b);
    return ok;
}

bool read_body(parsebuf.parsebuf_t *b, response_t *r) {
	const char *tmp = get_res_header(r, "Content-Length");
	if (tmp) {
		r->content_length = atoi(tmp);
		for (int i = 0; i < r->content_length; i++) {
			char c = parsebuf.buf_get(b);
			if (c == EOF) {
				return false;
			}
			r->body[i] = c;
		}
		return true;
	}

	// Connection: close implies that the rest of the data is body.
	tmp = get_res_header(r, "Connection");
	if (!strcmp(tmp, "close")) {
		size_t i = 0;
		while (parsebuf.buf_more(b)) {
			if (i + 1 == sizeof(r->body)) {
				panic("body buffer too small");
			}
			r->body[i++] = parsebuf.buf_get(b);
		}
		return true;
	}
	panic("unknown response body format");
	return false;
}

bool read_status_line(parsebuf.parsebuf_t *b, response_t *r) {
	// HTTP/1.1
	for (int i = 0; i < 8; i++) {
		r->version[i] = parsebuf.buf_get(b);
	}
	if (strcmp(r->version, "HTTP/1.0") && strcmp(r->version, "HTTP/1.1")) {
		return false;
	}

    // space
    if (parsebuf.buf_get(b) != ' ') return false;

	// 200
	r->status = 0;
	for (int i = 0; i < 3; i++) {
		char c = parsebuf.buf_get(b);
		if (!isdigit(c)) return false;
		r->status *= 10;
		r->status += c - '0';
	}

	// space
	if (parsebuf.buf_get(b) != ' ') return false;

	// status text
	while (parsebuf.buf_more(b) && parsebuf.buf_peek(b) != '\r') {
		parsebuf.buf_get(b);
	}

	// eol
	if (!parsebuf.buf_skip_literal(b, "\r\n")) {
		return false;
	}
	return true;
}

bool read_header(parsebuf.parsebuf_t *b, header_t *h) {
	char *tmp = h->name;
	while (parsebuf.buf_more(b) && parsebuf.buf_peek(b) != ':') {
		*tmp++ = parsebuf.buf_get(b);
	}
	// : space
	if (!parsebuf.buf_skip_literal(b, ": ")) {
		return false;
	}
	// value
	tmp = h->value;
	while (parsebuf.buf_more(b) && parsebuf.buf_peek(b) != '\r') {
		*tmp++ = parsebuf.buf_get(b);
	}
	// eol
	if (!parsebuf.buf_skip_literal(b, "\r\n")) {
		return false;
	}
	return true;
}
