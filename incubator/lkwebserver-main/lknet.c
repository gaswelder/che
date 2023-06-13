#import lklib.c
#import lkbuffer.c
#import request.c

enum {FD_SOCK, FD_FILE};

// Function return values:
// Z_OPEN (fd still open)
// Z_EOF (end of file)
// Z_ERR (errno set with error detail)
// Z_BLOCK (fd blocked, no data)
pub enum {
    Z_OPEN = 1,
    Z_EOF = 0,
    Z_ERR = -1,
    Z_BLOCK = -2
};

// Remove trailing CRLF or LF (\n) from string.
pub void lk_chomp(char* s) {
    int slen = strlen(s);
    for (int i=slen-1; i >= 0; i--) {
        if (s[i] != '\n' && s[i] != '\r') {
            break;
        }
        // Replace \n and \r with null chars. 
        s[i] = '\0';
    }
}

pub void lk_set_sock_nonblocking(int sock) {
    fcntl(sock, F_SETFL, O_NONBLOCK);
}

pub int nonblocking_error(int z) {
    if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return 1;
    }
    return 0;
}

// Read nonblocking fd count bytes to buf.
// Returns one of the following:
//    1 (Z_OPEN) for socket open
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
// On return, nbytes contains the number of bytes read.
pub int lk_read(int fd, int fd_type, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    int z;
    char readbuf[LK_BUFSIZE_LARGE];

    size_t nread = 0;
    while (nread < count) {
        // read minimum of sizeof(readbuf) and count-nread
        int nblock = count-nread;
        if (nblock > sizeof(readbuf)) nblock = sizeof(readbuf);

        if (fd_type == FD_SOCK) {
            z = recv(fd, readbuf, nblock, MSG_DONTWAIT);
        } else {
            z = read(fd, readbuf, nblock);
        }
        // EOF
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        lk_buffer_append(buf, readbuf, z);
        nread += z;
    }
    if (z > 0) {
        z = Z_OPEN;
    }
    *nbytes = nread;
    return z;
}
pub int lk_read_sock(int fd, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    return lk_read(fd, FD_SOCK, buf, count, nbytes);
}
pub int lk_read_file(int fd, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    return lk_read(fd, FD_FILE, buf, count, nbytes);
}

// Read all available nonblocking fd bytes to buffer.
// Returns one of the following:
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
//
// Note: This keeps track of last buf position read.
// Used to cumulatively read data into buf.
pub int lk_read_all(int fd, int fd_type, lkbuffer.LKBuffer *buf) {
    int z;
    char readbuf[LK_BUFSIZE_LARGE];
    while (1) {
        if (fd_type == FD_SOCK) {
            z = recv(fd, readbuf, sizeof(readbuf), MSG_DONTWAIT);
        } else {
            z = read(fd, readbuf, sizeof(readbuf));
        }
        // EOF
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        lk_buffer_append(buf, readbuf, z);
    }
    assert(z <= 0);
    return z;
}
pub int lk_read_all_sock(int fd, lkbuffer.LKBuffer *buf) {
    return lk_read_all(fd, FD_SOCK, buf);
}
pub int lk_read_all_file(int fd, lkbuffer.LKBuffer *buf) {
    return lk_read_all(fd, FD_FILE, buf);
}


// Write count buf bytes to nonblocking fd.
// Returns one of the following:
//    1 (Z_OPEN) for socket open
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
// On return, nbytes contains the number of bytes written.
//
// Note: use open(O_NONBLOCK) to open nonblocking file.
pub int lk_write(int fd, int fd_type, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    int z;
    if (count > buf->bytes_len) {
        count = buf->bytes_len;
    }
    size_t nwrite = 0;
    while (nwrite < count) {
        if (fd_type == FD_SOCK) {
            z = send(fd, buf->bytes+nwrite, count-nwrite, MSG_DONTWAIT | MSG_NOSIGNAL);
        } else {
            z = write(fd, buf->bytes+nwrite, count-nwrite);
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z >= 0);
        nwrite += z;
    }
    if (z >= 0) {
        z = Z_OPEN;
    }
    *nbytes = nwrite;
    return z;
}

pub int lk_write_sock(int fd, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    return lk_write(fd, FD_SOCK, buf, count, nbytes);
}

pub int lk_write_file(int fd, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    return lk_write(fd, FD_FILE, buf, count, nbytes);
}

// Write buf bytes to nonblocking fd.
// Returns one of the following:
//    0 (Z_EOF) for all buf bytes sent
//    1 (Z_OPEN) for socket open
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
//
// Note: This keeps track of last buf position written.
// Used to cumulatively write data into buf.
pub int lk_write_all(int fd, int fd_type, lkbuffer.LKBuffer *buf) {
    int z;
    while (1) {
        int nwrite = buf->bytes_len - buf->bytes_cur;
        if (nwrite <= 0) {
            z = Z_EOF;
            break;
        }
        if (fd_type == FD_SOCK) {
            z = send(fd,
                     buf->bytes + buf->bytes_cur, 
                     buf->bytes_len - buf->bytes_cur,
                     MSG_DONTWAIT | MSG_NOSIGNAL);
        } else {
            z = write(fd,
                      buf->bytes + buf->bytes_cur, 
                      buf->bytes_len - buf->bytes_cur);
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z >= 0);
        buf->bytes_cur += z;
    }
    if (z > 0) {
        z = Z_OPEN;
    }
    return z;
}
pub int lk_write_all_sock(int fd, lkbuffer.LKBuffer *buf) {
    return lk_write_all(fd, FD_SOCK, buf);
}
pub int lk_write_all_file(int fd, lkbuffer.LKBuffer *buf) {
    return lk_write_all(fd, FD_FILE, buf);
}



/** lksocketreader functions **/

pub LKSocketReader *lk_socketreader_new(int sock, size_t buf_size) {
    LKSocketReader *sr = lk_malloc(sizeof(LKSocketReader), "lk_socketreader_new");
    if (buf_size == 0) {
        buf_size = 1024;
    }
    sr->sock = sock;
    sr->buf = lk_buffer_new(buf_size);
    sr->sockclosed = 0;
    return sr;
}



pub void debugprint_buf(char *buf, size_t buf_size) {
    printf("buf: ");
    for (int i=0; i < buf_size; i++) {
        putchar(buf[i]);
    }
    putchar('\n');
    printf("buf_size: %ld\n", buf_size);
}




/*** request.LKHttpRequest functions ***/
pub request.LKHttpRequest *lk_httprequest_new() {
    request.LKHttpRequest *req = lk_malloc(sizeof(request.LKHttpRequest), "lk_httprequest_new");
    req->method = lk_string_new("");
    req->uri = lk_string_new("");
    req->path = lk_string_new("");
    req->filename = lk_string_new("");
    req->querystring = lk_string_new("");
    req->version = lk_string_new("");
    req->headers = lk_stringtable_new();
    req->head = lk_buffer_new(0);
    req->body = lk_buffer_new(0);
    return req;
}

pub void lk_httprequest_free(request.LKHttpRequest *req) {
    lkstring.lk_string_free(req->method);
    lkstring.lk_string_free(req->uri);
    lkstring.lk_string_free(req->path);
    lkstring.lk_string_free(req->filename);
    lkstring.lk_string_free(req->querystring);
    lkstring.lk_string_free(req->version);
    lk_stringtable_free(req->headers);
    lk_buffer_free(req->head);
    lk_buffer_free(req->body);

    req->method = NULL;
    req->uri = NULL;
    req->path = NULL;
    req->filename = NULL;
    req->querystring = NULL;
    req->version = NULL;
    req->headers = NULL;
    req->head = NULL;
    req->body = NULL;
    lk_free(req);
}

pub void lk_httprequest_add_header(request.LKHttpRequest *req, char *k, char *v) {
    lk_stringtable_set(req->headers, k, v);
}

pub void lk_httprequest_append_body(request.LKHttpRequest *req, char *bytes, int bytes_len) {
    lk_buffer_append(req->body, bytes, bytes_len);
}

pub void lk_httprequest_finalize(request.LKHttpRequest *req) {
    lk_buffer_clear(req->head);

    // Default to HTTP version.
    if (lk_string_sz_equal(req->version, "")) {
        lk_string_assign(req->version, "HTTP/1.0");
    }
    lk_buffer_append_sprintf(req->head, "%s %s %s\n", req->method->s, req->uri->s, req->version->s);
    if (req->body->bytes_len > 0) {
        lk_buffer_append_sprintf(req->head, "Content-Length: %ld\n", req->body->bytes_len);
    }
    for (int i=0; i < req->headers->items_len; i++) {
        lk_buffer_append_sprintf(req->head, "%s: %s\n", req->headers->items[i].k->s, req->headers->items[i].v->s);
    }
    lk_buffer_append(req->head, "\r\n", 2);
}


pub void lk_httprequest_debugprint(request.LKHttpRequest *req) {
    assert(req->method != NULL);
    assert(req->uri != NULL);
    assert(req->version != NULL);
    assert(req->head != NULL);
    assert(req->body != NULL);

    printf("method: %s\n", req->method->s);
    printf("uri: %s\n", req->uri->s);
    printf("version: %s\n", req->version->s);

    printf("headers_size: %ld\n", req->headers->items_size);
    printf("headers_len: %ld\n", req->headers->items_len);

    printf("Headers:\n");
    for (int i=0; i < req->headers->items_len; i++) {
        LKString *v = req->headers->items[i].v;
        printf("%s: %s\n", req->headers->items[i].k->s, v->s);
    }

    printf("Body:\n---\n");
    for (int i=0; i < req->body->bytes_len; i++) {
        putchar(req->body->bytes[i]);
    }
    printf("\n---\n");
}


/** httpresp functions **/
pub request.LKHttpResponse *lk_httpresponse_new() {
    request.LKHttpResponse *resp = lk_malloc(sizeof(request.LKHttpResponse), "lk_httpresponse_new");
    resp->status = 0;
    resp->statustext = lk_string_new("");
    resp->version = lk_string_new("");
    resp->headers = lk_stringtable_new();
    resp->head = lk_buffer_new(0);
    resp->body = lk_buffer_new(0);
    return resp;
}

pub void lk_httpresponse_free(request.LKHttpResponse *resp) {
    lkstring.lk_string_free(resp->statustext);
    lkstring.lk_string_free(resp->version);
    lk_stringtable_free(resp->headers);
    lk_buffer_free(resp->head);
    lk_buffer_free(resp->body);

    resp->statustext = NULL;
    resp->version = NULL;
    resp->headers = NULL;
    resp->head = NULL;
    resp->body = NULL;
    lk_free(resp);
}

pub void lk_httpresponse_add_header(request.LKHttpResponse *resp, char *k, char *v) {
    lk_stringtable_set(resp->headers, k, v);
}

// Finalize the http response by setting head buffer.
// Writes the status line, headers and CRLF blank string to head buffer.
pub void lk_httpresponse_finalize(request.LKHttpResponse *resp) {
    lk_buffer_clear(resp->head);

    // Default to 200 OK if no status set.
    if (resp->status == 0) {
        resp->status = 200;
        lk_string_assign(resp->statustext, "OK");
    }
    // Default to HTTP version.
    if (lk_string_sz_equal(resp->version, "")) {
        lk_string_assign(resp->version, "HTTP/1.0");
    }
    lk_buffer_append_sprintf(resp->head, "%s %d %s\n", resp->version->s, resp->status, resp->statustext->s);
    lk_buffer_append_sprintf(resp->head, "Content-Length: %ld\n", resp->body->bytes_len);
    for (int i=0; i < resp->headers->items_len; i++) {
        lk_buffer_append_sprintf(resp->head, "%s: %s\n", resp->headers->items[i].k->s, resp->headers->items[i].v->s);
    }
    lk_buffer_append(resp->head, "\r\n", 2);
}

pub void lk_httpresponse_debugprint(request.LKHttpResponse *resp) {
    assert(resp->statustext != NULL);
    assert(resp->version != NULL);
    assert(resp->headers != NULL);
    assert(resp->head);
    assert(resp->body);

    printf("status: %d\n", resp->status);
    printf("statustext: %s\n", resp->statustext->s);
    printf("version: %s\n", resp->version->s);
    printf("headers_size: %ld\n", resp->headers->items_size);
    printf("headers_len: %ld\n", resp->headers->items_len);

    printf("Headers:\n");
    for (int i=0; i < resp->headers->items_len; i++) {
        LKString *v = resp->headers->items[i].v;
        printf("%s: %s\n", resp->headers->items[i].k->s, v->s);
    }

    printf("Body:\n---\n");
    for (int i=0; i < resp->body->bytes_len; i++) {
        putchar(resp->body->bytes[i]);
    }
    printf("\n---\n");
}

// Open and read entire file contents into buf.
// Return number of bytes read or -1 for error.
pub ssize_t lk_readfile(char *filepath, lkbuffer.LKBuffer *buf) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    int z = lk_readfd(fd, buf);
    if (z == -1) {
        int tmperrno = errno;
        close(fd);
        errno = tmperrno;
        return z;
    }

    close(fd);
    return z;
}

// Read entire file descriptor contents into buf.
// Return number of bytes read or -1 for error.
#define TMPBUF_SIZE 512
pub ssize_t lk_readfd(int fd, lkbuffer.LKBuffer *buf) {
    char tmpbuf[TMPBUF_SIZE];

    int nread = 0;
    while (1) {
        int z = read(fd, tmpbuf, TMPBUF_SIZE);
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1) {
            return z;
        }
        if (z == 0) {
            break;
        }
        lk_buffer_append(buf, tmpbuf, z);
        nread += z;
    }
    return nread;
}

// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
pub char *lk_astrncat(char *dest, char *src, size_t src_len) {
    int dest_len = strlen(dest);
    dest = lk_realloc(dest, dest_len + src_len + 1, "lk_astrncat");
    strncat(dest, src, src_len);
    return dest;
}
