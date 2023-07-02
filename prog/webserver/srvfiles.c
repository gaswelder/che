#import fs
#import http
#import ioroutine
#import mime
#import os/io
#import strings

#import context.c
#import srvstd.c


const char *default_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "default.htm"
};

enum {
    RESOLVE,
    READ_FILE,
    WRITE_OUT,
    FLUSH
};

pub int client_routine(void *_ctx, int line) {
    context.ctx_t *ctx = _ctx;
    http.request_t *req = &ctx->req;

    switch (line) {
    /*
     * Locate and open the requested file.
     */
    case RESOLVE:
        printf("resolving %s\n", req->path);
        char *filepath = resolve_path(ctx->hc->homedir, req->path);
        if (!filepath) {
            printf("file \"%s\" not found\n", req->path);
            srvstd.write_404(req, ctx);
            return FLUSH;
        }
        const char *ext = fs.fileext(filepath);
        const char *content_type = mime.lookup(ext);
        if (content_type == NULL) {
            content_type = "text/plain";
        }
        size_t filesize = 0;
        if (!fs.filesize(filepath, &filesize)) {
            panic("failed to get file size");
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
        ctx->filehandle = io.open(filepath, "rb");
        if (!ctx->filehandle) {
            panic("oops");
        }
        printf("resolved file request to fd %d\n", ctx->filehandle->fd);
        return READ_FILE;

    /*
     * Read a chunk of file into the output buffer.
     */
    case READ_FILE:
        if (!ioroutine.ioready(ctx->filehandle, io.READ)) {
            return READ_FILE;
        }
        if (!io.read(ctx->filehandle, ctx->outbuf)) {
            panic("read failed: %s\n", strerror(errno));
        }
        size_t n = io.bufsize(ctx->outbuf);
        if (n == 0) {
            // Reached the end of file.
            io.close(ctx->filehandle);
            ctx->filehandle = NULL;
            return FLUSH;
        }
        return WRITE_OUT;
    
    /*
     * Write the chunk in the output buffer to the client.
     */
    case WRITE_OUT:
        // If nothing to write, go read.
        if (io.bufsize(ctx->outbuf) == 0) {
            return READ_FILE;
        }
        if (!ioroutine.ioready(ctx->client_handle, io.WRITE)) {
            return WRITE_OUT;
        }
        if (!io.write(ctx->client_handle, ctx->outbuf)) {
            panic("write failed: %s\n", strerror(errno));
        }
        return READ_FILE;
    
    /*
     * Write whatever's left in the output buffer and terminate the routine.
     */
    case FLUSH:
        if (io.bufsize(ctx->outbuf) == 0) {
            return -1;
        }
        if (!ioroutine.ioready(ctx->client_handle, io.WRITE)) {
            return FLUSH;
        }
        if (!io.write(ctx->client_handle, ctx->outbuf)) {
            panic("write failed: %s\n", strerror(errno));
        }
        return -1;
    }

    panic("unexpected line %d", line);
}

char *resolve_path(const char *homedir, *reqpath) {
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
