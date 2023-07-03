#import os/net

#define _XOPEN_SOURCE 700
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#type fd_set
#type socklen_t

enum {
    TYPE_FD,
    TYPE_NET,
    TYPE_FILE
};

pub typedef {
    char data[4096];
    size_t size;
} buf_t;

pub buf_t *newbuf() {
    buf_t *b = calloc(1, sizeof(buf_t));
    if (!b) {
        return NULL;
    }
    return b;
}

pub void print_buf(buf_t *b) {
    char tmp[4096] = {0};
    memcpy(tmp, b->data, b->size);
    fprintf(stderr, "%zu bytes: %s\n", b->size, tmp);
}

pub void freebuf(buf_t *b) {
    free(b);
}

pub size_t bufsize(buf_t *b) {
    return b->size;
}

pub size_t bufspace(buf_t *b) {
    return 4096 - b->size;
}

pub bool pushf(buf_t *b, const char *fmt, ...) {
    va_list args = {0};

    // Calculate the length.
	va_start(args, fmt);
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

    // If the string is too long, fail.
    if ((size_t) len > bufspace(b)) {
        return false;
    }

    // Print the string and adjust the pointers.
	va_start(args, fmt);
	vsprintf(b->data + b->size, fmt, args);
	va_end(args);
    b->size += len;
    return true;
}

pub bool push(buf_t *b, char *data, size_t n) {
    if (n > bufspace(b)) {
        printf("no space left in buffer\n");
        return false;
    }
    memcpy(b->data + b->size, data, n);
    b->size += n;
    return true;
}

pub void shift(buf_t *b, size_t n) {
    if (n == 0) {
        return;
    }
    if (n > b->size) {
        panic("can't shift by %zu, have only %zu\n", n, b->size);
    }
    memmove(b->data, b->data + n, b->size - n);
    b->size -= n;
}



pub typedef {
    int type;
    int fd;
    net.net_t *conn;
    FILE *file;
} handle_t;

pub int id(handle_t *h) {
    return h->fd;
}

pub handle_t *fdhandle(int fd) {
    handle_t *h = calloc(1, sizeof(handle_t));
    if (!h) {
        return NULL;
    }
    h->type = TYPE_FD;
    h->fd = fd;
    return h;
}

pub FILE *asfile(handle_t *h, const char *mode) {
    if (h->type == TYPE_FD) {
        return OS.fdopen(h->fd, mode);
    }
    panic("unhandled handle type: %d", h->type);
}

pub bool read(handle_t *h, buf_t *b) {
    size_t n = bufspace(b);
    if (n == 0) {
        return false;
    }
    int r = OS.read(h->fd, b->data + b->size, n);
    if (r < 0) {
        return false;
    }
    b->size += r;
    return true;
}

pub bool write(handle_t *h, buf_t *b) {
    if (b->size == 0) {
        return false;
    }
    int r = OS.write(h->fd, b->data, b->size);
    if (r < 0) {
        return false;
    }
    shift(b, (size_t) r);
    return true;
}

pub handle_t *connect(const char *proto, const char *addr) {
    handle_t *h = calloc(1, sizeof(handle_t));
    if (!h) {
        return NULL;
    }

    net.net_t *conn = net.net_open(proto, addr);
    if (!conn) {
        free(h);
        return NULL;
    }

    h->type = TYPE_NET;
    h->conn = conn;
    h->fd = conn->fd;
    return h;
}

pub handle_t *listen(const char *proto, *addr) {
    handle_t *h = calloc(1, sizeof(handle_t));
    if (!h) {
        return NULL;
    }
    net.net_t *ln = net.net_listen(proto, addr);
    if (!ln) {
        free(h);
        return NULL;
    }
    h->type = TYPE_NET;
    h->conn = ln;
    h->fd = ln->fd;
    return h;
}

pub handle_t *accept(handle_t *ln) {
    if (ln->type != TYPE_NET) {
        return NULL;
    }
    handle_t *h = calloc(1, sizeof(handle_t));
    if (!h) {
        return NULL;
    }
    net.net_t *conn = net.net_accept(ln->conn);
    if (!conn) {
        free(h);
        return NULL;
    }
    h->type = TYPE_NET;
    h->conn = conn;
    h->fd = conn->fd;
    return h;
}

pub handle_t *open(const char *path, *flags) {
    FILE *f = fopen(path, flags);
    if (!f) {
        return NULL;
    }
    handle_t *h = calloc(1, sizeof(handle_t));
    if (!h) {
        return NULL;
    }
    h->type = TYPE_FILE;
    h->file = f;
    h->fd = OS.fileno(f);
    return h;
}

pub bool close(handle_t *h) {
    if (h->type == TYPE_FILE) {
        if (fclose(h->file) != 0) {
            return false;
        }
        free(h);
        return true;
    }
    if (h->type == TYPE_NET) {
        net.net_close(h->conn);
        free(h);
        return true;
    }
    if (h->type == TYPE_FD) {
        OS.close(h->fd);
        return true;
    }
    panic("unhandled type");
}


pub enum {
    READ = 1,
    WRITE = 2
};

pub typedef {
    handle_t *handle;
    bool readable;
    bool writable;
} event_t;

pub typedef {
    handle_t *handles[1000];
    int filters[1000];
    event_t result[1000];
} pool_t;

pub pool_t *newpool() {
    pool_t *p = calloc(1, sizeof(pool_t));
    if (!p) {
        return NULL;
    }
    return p;
}

pub void resetpool(pool_t *p) {
    for (int i = 0; i < 1000; i++) {
        p->handles[i] = NULL;
        p->result[i].handle = NULL;
    }
}

pub void freepool(pool_t *p) {
    free(p);
}

pub void add(pool_t *p, handle_t *h, int filter) {
    // See if this handle is already in the list.
    for (size_t i = 0; i < nelem(p->handles); i++) {
        if (p->handles[i] == h) {
            p->filters[i] |= filter;
            return;
        }
    }
    // If not, add a new entry.
    for (size_t i = 0; i < nelem(p->handles); i++) {
        if (!p->handles[i]) {
            p->handles[i] = h;
            p->filters[i] = filter;
            return;
        }
    }
    panic("failed to add a handle");
}

pub event_t *poll(pool_t *p) {
    fd_set readset = {0};
    fd_set writeset = {0};
    int max = 0;
    for (size_t i = 0; i < nelem(p->handles); i++) {
        handle_t *h = p->handles[i];
        if (!h) {
            continue;
        }
        if (p->filters[i] & READ) {
            OS.FD_SET(h->fd, &readset);
        }
        if (p->filters[i] & WRITE) {
            OS.FD_SET(h->fd, &writeset);
        }
        if (h->fd > max) {
            max = h->fd;
        }
    }
    int r = OS.select(max + 1, &readset, &writeset, NULL, NULL);
    if (r < 0) {
        return false;
    }
    int size = 0;
    for (size_t i = 0; i < nelem(p->handles); i++) {
        handle_t *h = p->handles[i];
        if (!h) {
            continue;
        }
        bool r = p->filters[i] & READ && OS.FD_ISSET(h->fd, &readset);
        bool w = p->filters[i] & WRITE && OS.FD_ISSET(h->fd, &writeset);
        if (!r && !w) {
            continue;
        }
        p->result[size].handle = h;
        p->result[size].readable = r;
        p->result[size].writable = w;
        size++;
    }
    p->result[size].handle = NULL;
    return p->result;
}
