#import http
#import os/io

#import lkcontext.c

pub void write_404(http.request_t *req, lkcontext.LKContext *ctx) {
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

pub void write_405(http.request_t *req, lkcontext.LKContext *ctx) {
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

pub void write_501(http.request_t *req, lkcontext.LKContext *ctx) {
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
