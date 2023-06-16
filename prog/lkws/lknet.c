#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#import lkbuffer.c
#import lkstring.c
#import request.c
#import lkstringtable.c

#define LK_BUFSIZE_LARGE 2048

enum {FD_SOCK, FD_FILE};

pub enum {
    Z_OPEN = 1, // fd still open
    Z_EOF = 0, // end of file
    Z_ERR = -1, // error, check errno
    Z_BLOCK = -2 // fd blocked, no data
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

// Read nonblocking fd count bytes to buf.
// Returns one of the following:
//    1 (Z_OPEN) for socket OS.open
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
        if ((size_t)nblock > sizeof(readbuf)) nblock = sizeof(readbuf);

        if (fd_type == FD_SOCK) {
            z = OS.recv(fd, readbuf, nblock, OS.MSG_DONTWAIT);
        } else {
            z = OS.read(fd, readbuf, nblock);
        }
        // EOF
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        lkbuffer.lk_buffer_append(buf, readbuf, z);
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
            z = OS.recv(fd, readbuf, sizeof(readbuf), OS.MSG_DONTWAIT);
        } else {
            z = OS.read(fd, readbuf, sizeof(readbuf));
        }
        // EOF
        if (z == 0) {
            z = Z_EOF;
            break;
        }
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
            z = Z_BLOCK;
            break;
        }
        if (z == -1) {
            // errno is set to EPIPE if socket was shutdown
            z = Z_ERR;
            break;
        }
        assert(z > 0);
        lkbuffer.lk_buffer_append(buf, readbuf, z);
    }
    assert(z <= 0);
    return z;
}

pub int lk_read_all_file(int fd, lkbuffer.LKBuffer *buf) {
    return lk_read_all(fd, FD_FILE, buf);
}


// Write count buf bytes to nonblocking fd.
// Returns one of the following:
//    1 (Z_OPEN) for socket OS.open
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
// On return, nbytes contains the number of bytes written.
//
// Note: use OS.open(O_NONBLOCK) to OS.open nonblocking file.
pub int lk_write(int fd, int fd_type, lkbuffer.LKBuffer *buf, size_t count, size_t *nbytes) {
    int z;
    if (count > buf->bytes_len) {
        count = buf->bytes_len;
    }
    size_t nwrite = 0;
    while (nwrite < count) {
        if (fd_type == FD_SOCK) {
            z = OS.send(fd, buf->bytes+nwrite, count-nwrite, OS.MSG_DONTWAIT | OS.MSG_NOSIGNAL);
        } else {
            z = OS.write(fd, buf->bytes+nwrite, count-nwrite);
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
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
// Returns one of the Z_* statuses.
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
            z = OS.send(fd,
                     buf->bytes + buf->bytes_cur, 
                     buf->bytes_len - buf->bytes_cur,
                     OS.MSG_DONTWAIT | OS.MSG_NOSIGNAL);
        } else {
            z = OS.write(fd,
                      buf->bytes + buf->bytes_cur, 
                      buf->bytes_len - buf->bytes_cur);
        }
        // interrupt occured during read, retry read.
        if (z == -1 && errno == OS.EINTR) {
            continue;
        }
        if (z == -1 && (errno == OS.EAGAIN || errno == OS.EWOULDBLOCK)) {
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


/*** request.LKHttpRequest functions ***/


pub void lk_httprequest_finalize(request.LKHttpRequest *req) {
    lkbuffer.lk_buffer_clear(req->head);

    // Default to HTTP version.
    if (!strcmp(req->version, "")) {
        strcpy(req->version, "HTTP/1.0");
    }
    lkbuffer.lk_buffer_append_sprintf(req->head, "%s %s %s\n", req->method, req->uri, req->version);
    if (req->body->bytes_len > 0) {
        lkbuffer.lk_buffer_append_sprintf(req->head, "Content-Length: %ld\n", req->body->bytes_len);
    }
    for (size_t i=0; i < req->headers->items_len; i++) {
        lkbuffer.lk_buffer_append_sprintf(req->head, "%s: %s\n", req->headers->items[i].k->s, req->headers->items[i].v->s);
    }
    lkbuffer.lk_buffer_append(req->head, "\r\n", 2);
}


pub void lk_httprequest_debugprint(request.LKHttpRequest *req) {
    assert(req->head != NULL);
    assert(req->body != NULL);

    printf("method: %s\n", req->method);
    printf("uri: %s\n", req->uri);
    printf("version: %s\n", req->version);

    printf("headers_size: %ld\n", req->headers->items_size);
    printf("headers_len: %ld\n", req->headers->items_len);

    printf("Headers:\n");
    for (size_t i=0; i < req->headers->items_len; i++) {
        lkstring.LKString *v = req->headers->items[i].v;
        printf("%s: %s\n", req->headers->items[i].k->s, v->s);
    }

    printf("Body:\n---\n");
    for (size_t i=0; i < req->body->bytes_len; i++) {
        putchar(req->body->bytes[i]);
    }
    printf("\n---\n");
}


/** httpresp functions **/

pub void lk_httpresponse_add_header(request.LKHttpResponse *resp, const char *k, *v) {
    lkstringtable.lk_stringtable_set(resp->headers, k, v);
}

// Finalize the http response by setting head buffer.
// Writes the status line, headers and CRLF blank string to head buffer.
pub void lk_httpresponse_finalize(request.LKHttpResponse *resp) {
    lkbuffer.lk_buffer_clear(resp->head);

    // Default to 200 OK if no status set.
    if (resp->status == 0) {
        resp->status = 200;
        lkstring.lk_string_assign(resp->statustext, "OK");
    }
    // Default to HTTP version.
    if (lkstring.lk_string_sz_equal(resp->version, "")) {
        lkstring.lk_string_assign(resp->version, "HTTP/1.0");
    }
    lkbuffer.lk_buffer_append_sprintf(resp->head, "%s %d %s\n", resp->version->s, resp->status, resp->statustext->s);
    lkbuffer.lk_buffer_append_sprintf(resp->head, "Content-Length: %ld\n", resp->body->bytes_len);
    for (size_t i=0; i < resp->headers->items_len; i++) {
        lkbuffer.lk_buffer_append_sprintf(resp->head, "%s: %s\n", resp->headers->items[i].k->s, resp->headers->items[i].v->s);
    }
    lkbuffer.lk_buffer_append(resp->head, "\r\n", 2);
}

pub void lk_httpresponse_debugprint(request.LKHttpResponse *resp) {
    assert(resp->statustext != NULL);
    assert(resp->headers != NULL);
    assert(resp->head);
    assert(resp->body);

    printf("status: %d\n", resp->status);
    printf("statustext: %s\n", resp->statustext->s);
    printf("version: %s\n", resp->version->s);
    printf("headers_size: %ld\n", resp->headers->items_size);
    printf("headers_len: %ld\n", resp->headers->items_len);

    printf("Headers:\n");
    for (size_t i=0; i < resp->headers->items_len; i++) {
        lkstring.LKString *v = resp->headers->items[i].v;
        printf("%s: %s\n", resp->headers->items[i].k->s, v->s);
    }

    printf("Body:\n---\n");
    for (size_t i=0; i < resp->body->bytes_len; i++) {
        putchar(resp->body->bytes[i]);
    }
    printf("\n---\n");
}


// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
pub char *lk_astrncat(char *dest, char *src, size_t src_len) {
    int dest_len = strlen(dest);
    dest = realloc(dest, dest_len + src_len + 1);
    strncat(dest, src, src_len);
    return dest;
}
