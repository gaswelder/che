#import fs
#import http
#import ioroutine
#import mime
#import os/io
#import strings

#import context.c
#import lkhostconfig.c
#import srvstd.c


const char *default_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "default.htm"
};

pub int client_routine(void *_ctx, int line) {
    context.ctx_t *ctx = _ctx;
    switch (line) {
    case 0:
        http.request_t *req = &ctx->req;
        printf("resolving %s\n", req->path);
        char *filepath = resolve_path(ctx->hc->homedir, req->path);
        if (!filepath) {
            printf("file \"%s\" not found, serving 404\n", req->path);
            srvstd.write_404(req, ctx);
            return 3;
        }
        printf("resolved %s as %s\n", req->path, filepath);
        const char *ext = fs.fileext(filepath);
        const char *content_type = mime.lookup(ext);
        if (content_type == NULL) {
            printf("didn't get MIME type for \"%s\", using text/plain\n", ext);
            content_type = "text/plain";
        }
        size_t filesize = 0;
        if (!fs.filesize(filepath, &filesize)) {
            panic("failed to get file size");
        }
        printf("writing headers\n");
        if (!io.pushf(ctx->outbuf,
            "%s 200 OK\n"
            "Content-Length: %ld\n"
            "Content-Type: %s\n\n\n",
            req->version,
            filesize,
            content_type
        )) {
            panic("failed to write response headers");
        }
        ctx->filehandle = io.open(filepath, "rb");
        if (!ctx->filehandle) {
            panic("oops");
        }
        printf("resolved file request to fd %d\n", ctx->filehandle->fd);
        return 2;
    case 2:
        if (!ioroutine.ioready(ctx->filehandle, io.READ)) {
            return 2;
        }
        if (!io.read(ctx->filehandle, ctx->outbuf)) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            return -1;
        }
        size_t n = io.bufsize(ctx->outbuf);
        printf("read %zu from the file\n", n);
        if (n == 0) {
            printf("closing file, requested finished\n");
            printf("output has %zu\n", io.bufsize(ctx->outbuf));
            io.close(ctx->filehandle);
            ctx->filehandle = NULL;
            return -1;
        }
        return 3;
    case 3:
        if (!ioroutine.ioready(ctx->client_handle, io.WRITE)) {
            return 3;
        }
        if (!io.write(ctx->client_handle, ctx->outbuf)) {
            fprintf(stderr, "write failed: %s\n", strerror(errno));
            if (ctx->filehandle) {
                io.close(ctx->filehandle);
            }
            return -1;
        }
        return 2;
    }
    panic("unexpected line %d", line);
}

pub io.handle_t *resolve1(http.request_t *req, lkhostconfig.LKHostConfig *hc) {
    if (!strcmp(req->method, "GET")) {
        char *filepath = resolve_path(hc->homedir, req->path);
        if (!filepath) {
            printf("file \"%s\" not found, serving 404\n", req->path);
            // srvstd.write_404(req, ctx);
            // return true;
        }
        io.handle_t *h = io.open(filepath, "rb");
        free(filepath);
        return h;
    }
    panic("kek");
    // if (!strcmp(req->method, "POST")) {
    //     srvstd.write_405(req, ctx);
    //     return true;
    // }
    // srvstd.write_501(req, ctx);
    // return true;
}

pub char *resolve_path(const char *homedir, *reqpath) {
    if (!strcmp(reqpath, "")) {
        for (size_t i = 0; i < nelem(default_files); i++) {
            char *p = resolve_inner(homedir, default_files[i]);
            if (p) {
                return p;
            }
        }
        return NULL;
    }
    return resolve_inner(homedir, reqpath);
}

pub bool resolve(http.request_t *req, context.ctx_t *ctx, lkhostconfig.LKHostConfig *hc) {
    if (!strcmp(req->method, "GET")) {
        char *filepath = resolve_path(hc->homedir, req->path);
        if (!filepath) {
            printf("file \"%s\" not found, serving 404\n", req->path);
            srvstd.write_404(req, ctx);
            return true;
        }
        write_file(req, ctx, filepath);
        free(filepath);
        return true;
    }
    if (!strcmp(req->method, "POST")) {
        srvstd.write_405(req, ctx);
        return true;
    }
    srvstd.write_501(req, ctx);
    return true;
}



char *resolve_inner(const char *homedir, *reqpath) {
    char naive_path[4096] = {0};
    if (strlen(homedir) + strlen(reqpath) + 1 >= sizeof(naive_path)) {
        printf("ERROR: requested path is too long: %s\n", reqpath);
        return NULL;
    }
    sprintf(naive_path, "%s/%s", homedir, reqpath);

    char realpath[4096] = {0};
    if (!fs.realpath(naive_path, realpath, sizeof(realpath))) {
        return NULL;
    }
    if (!strings.starts_with(realpath, homedir)) {
        printf("resolved path \"%s\" doesn't start with homedir=\"%s\"\n", realpath, homedir);
        return NULL;
    }
    return strings.newstr("%s", realpath);
}

void write_file(http.request_t *req, context.ctx_t *ctx, const char *filepath) {
    const char *ext = fs.fileext(filepath);
    const char *content_type = mime.lookup(ext);
    if (content_type == NULL) {
        printf("didn't get MIME type for \"%s\", using text/plain\n", ext);
        content_type = "text/plain";
    }

    size_t filesize = 0;
    char *data = fs.readfile(filepath, &filesize);
    if (!data) {
        panic("failed to read file '%s'", filepath);
    }

    if (!io.pushf(ctx->outbuf,
        "%s 200 OK\n"
        "Content-Length: %ld\n"
        "Content-Type: %s\n\n\n",
        req->version,
        filesize,
        content_type
    )) {
        panic("failed to write response headers");
    }
    if (!io.push(ctx->outbuf, data, filesize)) {
        panic("failed to write data to the buffer");
    }
}
