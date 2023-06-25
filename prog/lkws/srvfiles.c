#import strings
#import fs
#import http
#import mime
#import os/io
#import fileutil

#import lkcontext.c
#import lkhostconfig.c

const char *default_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "default.htm"
};

pub bool resolve(http.request_t *req, lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    if (!strcmp(req->method, "GET")) {
        char *filepath = resolve_path(hc->homedir, req->path);
        if (!filepath) {
            printf("file \"%s\" not found, serving 404\n", req->path);
            write_404(req, ctx);
            ctx->type = lkcontext.CTX_WRITE_DATA;
            return true;
        }
        write_file(req, ctx, filepath);
        free(filepath);
        ctx->type = lkcontext.CTX_WRITE_DATA;
        return true;
    }
    if (!strcmp(req->method, "POST")) {
        write_405(req, ctx);
        return true;
    }
    write_501(req, ctx);
    return true;
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

void write_404(http.request_t *req, lkcontext.LKContext *ctx) {
    const char *msg = "File not found on the server.";
    bool ok = io.pushf(ctx->outbuf,
        "%s 404 Not Found\n"
        "Content-Length: %ld\n"
        "Content-Type: text/plain\n"
        "\n"
        "\n"
        "%s",
        req->version,
        strlen(msg),
        msg
    );
    if (!ok) {
        panic("failed to write to the output buffer");
    }
}

void write_405(http.request_t *req, lkcontext.LKContext *ctx) {
    const char *msg = "This method is not allowed for this path.";
    bool ok = io.pushf(ctx->outbuf,
        "%s 405 Method Not Allowed\n"
        "Content-Length: %ld\n"
        "Content-Type: text/plain\n"
        "\n"
        "\n"
        "%s",
        req->version,
        strlen(msg),
        msg
    );
    if (!ok) {
        panic("failed to write to the output buffer");
    }
}

void write_501(http.request_t *req, lkcontext.LKContext *ctx) {
    const char *msg = "method not implemented\n";
    bool ok = io.pushf(ctx->outbuf,
        "%s 501 Not Implemented\n"
        "Content-Length: %ld\n"
        "Content-Type: text/plain\n"
        "\n"
        "\n"
        "%s",
        req->version,
        strlen(msg),
        msg
    );
    if (!ok) {
        panic("failed to write to the output buffer");
    }
}

void write_file(http.request_t *req, lkcontext.LKContext *ctx, const char *filepath) {
    const char *ext = fileutil.fileext(filepath);
    const char *content_type = mime.lookup(ext);
    if (content_type == NULL) {
        printf("didn't get MIME type for \"%s\", using text/plain\n", ext);
        content_type = "text/plain";
    }

    size_t filesize = 0;
    char *data = fileutil.readfile(filepath, &filesize);
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
