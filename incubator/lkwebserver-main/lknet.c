// int lk_open_listen_socket(char *host, char *port, int backlog, struct sockaddr *psa) {
//     int z;

//     struct addrinfo hints, *ai;
//     memset(&hints, 0, sizeof(hints));
//     hints.ai_family = AF_INET;
//     hints.ai_socktype = SOCK_STREAM; //
//     hints.ai_flags = AI_PASSIVE;
//     z = getaddrinfo(host, port, &hints, &ai);
//     if (z != 0) {
//         printf("getaddrinfo(): %s\n", gai_strerror(z));
//         errno = EINVAL;
//         return -1;
//     }
//     if (psa != NULL) {
//         memcpy(psa, ai->ai_addr, ai->ai_addrlen);
//     }

//     int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
//     if (fd == -1) {
//         lk_print_err("socket()");
//         z = -1;
//         goto error_return;
//     }
//     int yes=1;
//     z = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
//     if (z == -1) {
//         lk_print_err("setsockopt()");
//         goto error_return;
//     }
//     z = bind(fd, ai->ai_addr, ai->ai_addrlen);
//     if (z == -1) {
//         lk_print_err("bind()");
//         goto error_return;
//     }
//     z = listen(fd, backlog);
//     if (z == -1) {
//         lk_print_err("listen()");
//         goto error_return;
//     }

//     freeaddrinfo(ai);
//     return fd;

// error_return:
//     freeaddrinfo(ai);
//     return z;
// }

// You can specify the host and port in two ways:
// 1. host="littlekitten.xyz", port="5001" (separate host and port)
// 2. host="littlekitten.xyz:5001", port="" (combine host:port in host parameter)
int lk_open_connect_socket(char *host, char *port, struct sockaddr *psa) {
    int z;

    // If host is of the form "host:port", parse it.
    LKString *lksport = lk_string_new(host);
    LKStringList *ss = lk_string_split(lksport, ":");
    if (ss->items_len == 2) {
        host = ss->items[0]->s;
        port = ss->items[1]->s;
    }

    struct addrinfo hints, *ai;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    z = getaddrinfo(host, port, &hints, &ai);
    if (z != 0) {
        printf("getaddrinfo(): %s\n", gai_strerror(z));
        errno = EINVAL;
        return -1;
    }
    if (psa != NULL) {
        memcpy(psa, ai->ai_addr, ai->ai_addrlen);
    }

    lk_stringlist_free(ss);
    lkstring.lk_string_free(lksport);

    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
        lk_print_err("socket()");
        z = -1;
        goto error_return;
    }
    int yes=1;
    z = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (z == -1) {
        lk_print_err("setsockopt()");
        goto error_return;
    }
    z = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (z == -1) {
        lk_print_err("connect()");
        goto error_return;
    }

    freeaddrinfo(ai);
    return fd;

error_return:
    freeaddrinfo(ai);
    return z;
}


// Remove trailing CRLF or LF (\n) from string.
void lk_chomp(char* s) {
    int slen = strlen(s);
    for (int i=slen-1; i >= 0; i--) {
        if (s[i] != '\n' && s[i] != '\r') {
            break;
        }
        // Replace \n and \r with null chars. 
        s[i] = '\0';
    }
}

void lk_set_sock_timeout(int sock, int nsecs, int ms) {
    struct timeval tv;
    tv.tv_sec = nsecs;
    tv.tv_usec = ms * 1000; // convert milliseconds to microseconds
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
}

void lk_set_sock_nonblocking(int sock) {
    fcntl(sock, F_SETFL, O_NONBLOCK);
}

// Return sin_addr or sin6_addr depending on address family.
void *sockaddr_sin_addr(struct sockaddr *sa) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) sa;
        return &(p->sin_addr);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) sa;
        return &(p->sin6_addr);
    }
}
// Return sin_port or sin6_port depending on address family.
unsigned short lk_get_sockaddr_port(struct sockaddr *sa) {
    // addr->ai_addr is either struct sockaddr_in* or sockaddr_in6* depending on ai_family
    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *p = (struct sockaddr_in*) sa;
        return ntohs(p->sin_port);
    } else {
        struct sockaddr_in6 *p = (struct sockaddr_in6*) sa;
        return ntohs(p->sin6_port);
    }
}

// Return human readable IP address from sockaddr
LKString *lk_get_ipaddr_string(struct sockaddr *sa) {
    char servipstr[INET6_ADDRSTRLEN];
    const char *pz = inet_ntop(sa->sa_family, sockaddr_sin_addr(sa),
                               servipstr, sizeof(servipstr));
    if (pz == NULL) {
        lk_print_err("inet_ntop()");
        return lk_string_new("");
    }
    return lk_string_new(servipstr);
}

int nonblocking_error(int z) {
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
int lk_read(int fd, FDType fd_type, LKBuffer *buf, size_t count, size_t *nbytes) {
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
int lk_read_sock(int fd, LKBuffer *buf, size_t count, size_t *nbytes) {
    return lk_read(fd, FD_SOCK, buf, count, nbytes);
}
int lk_read_file(int fd, LKBuffer *buf, size_t count, size_t *nbytes) {
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
int lk_read_all(int fd, FDType fd_type, LKBuffer *buf) {
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
int lk_read_all_sock(int fd, LKBuffer *buf) {
    return lk_read_all(fd, FD_SOCK, buf);
}
int lk_read_all_file(int fd, LKBuffer *buf) {
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
int lk_write(int fd, FDType fd_type, LKBuffer *buf, size_t count, size_t *nbytes) {
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
int lk_write_sock(int fd, LKBuffer *buf, size_t count, size_t *nbytes) {
    return lk_write(fd, FD_SOCK, buf, count, nbytes);
}
int lk_write_file(int fd, LKBuffer *buf, size_t count, size_t *nbytes) {
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
int lk_write_all(int fd, FDType fd_type, LKBuffer *buf) {
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
int lk_write_all_sock(int fd, LKBuffer *buf) {
    return lk_write_all(fd, FD_SOCK, buf);
}
int lk_write_all_file(int fd, LKBuffer *buf) {
    return lk_write_all(fd, FD_FILE, buf);
}

// Pipe all available nonblocking readfd bytes into writefd.
// Uses buf as buffer for queued up bytes waiting to be written.
// Returns one of the following:
//    0 (Z_EOF) for read/write complete.
//    1 (Z_OPEN) for writefd socket open
//   -1 (Z_ERR) for read/write error.
//   -2 (Z_BLOCK) for blocked readfd/writefd socket
int lk_pipe_all(int readfd, int writefd, FDType fd_type, LKBuffer *buf) {
    int readz, writez;

    readz = lk_read_all(readfd, fd_type, buf);
    if (readz == Z_ERR) {
        return readz;
    }
    assert(readz == Z_EOF || readz == Z_BLOCK);

    writez = lk_write_all(writefd, fd_type, buf);
    if (writez == Z_ERR) {
        return writez;
    }

    // Pipe complete if no more data to read and all bytes sent.
    if (readz == Z_EOF && writez == Z_EOF) {
        return Z_EOF;
    }

    if (readz == Z_BLOCK) {
        return Z_BLOCK;
    }
    if (writez == Z_EOF) {
        writez = Z_OPEN;
    }
    return writez;
}

/** lksocketreader functions **/

LKSocketReader *lk_socketreader_new(int sock, size_t buf_size) {
    LKSocketReader *sr = lk_malloc(sizeof(LKSocketReader), "lk_socketreader_new");
    if (buf_size == 0) {
        buf_size = 1024;
    }
    sr->sock = sock;
    sr->buf = lk_buffer_new(buf_size);
    sr->sockclosed = 0;
    return sr;
}

void lk_socketreader_free(LKSocketReader *sr) {
    lk_buffer_free(sr->buf);
    sr->buf = NULL;
    lk_free(sr);
}

// Read one line from buffered socket including the \n char if present.
// Function return values:
// Z_OPEN (fd still open)
// Z_EOF (end of file)
// Z_ERR (errno set with error detail)
// Z_BLOCK (fd blocked, no data)
int lk_socketreader_readline(LKSocketReader *sr, LKString *line) {
    int z = Z_OPEN;
    lk_string_assign(line, "");

    if (sr->sockclosed) {
        return Z_EOF;
    }
    LKBuffer *buf = sr->buf;

    while (1) { // leave space for null terminator
        // If no buffer chars available, read from socket.
        if (buf->bytes_cur >= buf->bytes_len) {
            memset(buf->bytes, '*', buf->bytes_size); // initialize for debugging purposes.
            z = recv(sr->sock, buf->bytes, buf->bytes_size, MSG_DONTWAIT | MSG_NOSIGNAL);
            // socket closed, no more data
            if (z == 0) {
                sr->sockclosed = 1;
                z = Z_EOF;
                break;
            }
            // any other error
            if (z == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return Z_BLOCK;
            }
            if (z == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                return Z_ERR;
            }
            assert(z > 0);
            buf->bytes_len = z;
            buf->bytes_cur = 0;
            z = Z_OPEN;
        }

        // Copy unread buffer bytes into dst until a '\n' char.
        while (1) {
            if (buf->bytes_cur >= buf->bytes_len) {
                break;
            }
            char ch = buf->bytes[buf->bytes_cur];
            lk_string_append_char(line, ch);
            buf->bytes_cur++;

            if (ch == '\n') {
                goto readline_end;
            }
        }
    }

readline_end:
    assert(z <= Z_OPEN);
    return z;
}

int lk_socketreader_recv(LKSocketReader *sr, LKBuffer *buf_dest) {
    lk_buffer_clear(buf_dest);
    LKBuffer *buf = sr->buf;

    // Append any unread buffer bytes into buf_dest.
    if (buf->bytes_cur < buf->bytes_len) {
        int ncopy = buf->bytes_len - buf->bytes_cur;
        lk_buffer_append(buf_dest, buf->bytes + buf->bytes_cur, ncopy);
        buf->bytes_cur += ncopy;
    }

    int z = lk_read_all_sock(sr->sock, buf_dest);
    if (z == Z_EOF) {
        sr->sockclosed = 1;
    }
    return z;
}

void debugprint_buf(char *buf, size_t buf_size) {
    printf("buf: ");
    for (int i=0; i < buf_size; i++) {
        putchar(buf[i]);
    }
    putchar('\n');
    printf("buf_size: %ld\n", buf_size);
}

void lk_socketreader_debugprint(LKSocketReader *sr) {
    printf("buf_size: %ld\n", sr->buf->bytes_size);
    printf("buf_len: %ld\n", sr->buf->bytes_len);
    printf("buf cur: %ld\n", sr->buf->bytes_cur);
    printf("sockclosed: %d\n", sr->sockclosed);
    debugprint_buf(sr->buf->bytes, sr->buf->bytes_size);
    printf("\n");
}


/*** LKHttpRequest functions ***/
LKHttpRequest *lk_httprequest_new() {
    LKHttpRequest *req = lk_malloc(sizeof(LKHttpRequest), "lk_httprequest_new");
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

void lk_httprequest_free(LKHttpRequest *req) {
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

void lk_httprequest_add_header(LKHttpRequest *req, char *k, char *v) {
    lk_stringtable_set(req->headers, k, v);
}

void lk_httprequest_append_body(LKHttpRequest *req, char *bytes, int bytes_len) {
    lk_buffer_append(req->body, bytes, bytes_len);
}

void lk_httprequest_finalize(LKHttpRequest *req) {
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


void lk_httprequest_debugprint(LKHttpRequest *req) {
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
LKHttpResponse *lk_httpresponse_new() {
    LKHttpResponse *resp = lk_malloc(sizeof(LKHttpResponse), "lk_httpresponse_new");
    resp->status = 0;
    resp->statustext = lk_string_new("");
    resp->version = lk_string_new("");
    resp->headers = lk_stringtable_new();
    resp->head = lk_buffer_new(0);
    resp->body = lk_buffer_new(0);
    return resp;
}

void lk_httpresponse_free(LKHttpResponse *resp) {
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

void lk_httpresponse_add_header(LKHttpResponse *resp, char *k, char *v) {
    lk_stringtable_set(resp->headers, k, v);
}

// Finalize the http response by setting head buffer.
// Writes the status line, headers and CRLF blank string to head buffer.
void lk_httpresponse_finalize(LKHttpResponse *resp) {
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

void lk_httpresponse_debugprint(LKHttpResponse *resp) {
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
ssize_t lk_readfile(char *filepath, LKBuffer *buf) {
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
ssize_t lk_readfd(int fd, LKBuffer *buf) {
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
char *lk_astrncat(char *dest, char *src, size_t src_len) {
    int dest_len = strlen(dest);
    dest = lk_realloc(dest, dest_len + src_len + 1, "lk_astrncat");
    strncat(dest, src, src_len);
    return dest;
}

// Return whether file exists.
int lk_file_exists(char *filename) {
    struct stat statbuf;
    int z = stat(filename, &statbuf);
    return !z;
}

