pub typedef {
    char name[1024];
    char value[1024];
} header_t;

pub typedef {
    header_t headers[100];
    size_t nheaders;
} head_t;

pub size_t parse_head(head_t *r, char *data, size_t n) {
    size_t pos = 0;
    while (true) {
        char name[1000] = {0};
        char *d = name;

        // name
        while (pos < n && data[pos] != ':') {
            *d++ = data[pos];
            pos++;
        }
        if (pos >= n) return false;

        // skip ':'
        pos++;

        // skip spaces
        while (pos < n && isspace(data[pos])) {
            pos++;
        }
        if (pos >= n) return false;

        char value[1000] = {0};
        d = value;

        // everything until a newline
        while (pos < n && data[pos] != '\n' && data[pos] != '\r') {
            *d++ = data[pos];
            pos++;
        }
        if (pos >= n) return false;
        if (pos < n && data[pos] == '\r') pos++;
        if (pos < n && data[pos] == '\n') pos++;
        strcpy(r->headers[r->nheaders].name, name);
        strcpy(r->headers[r->nheaders].value, value);
        r->nheaders++;
        if (pos < n && (data[pos] == '\r' || data[pos] == '\n')) {
            if (pos < n && data[pos] == '\r') pos++;
            if (pos < n && data[pos] == '\n') pos++;
            return pos;
        }
    }
}
