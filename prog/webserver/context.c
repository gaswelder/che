#import http
#import os/exec
#import os/io

#import config.c

pub typedef {
    io.handle_t *client_handle; // Connected client.
    io.buf_t *inbuf, *outbuf; // Buffers for incoming and outgoing data.
    http.request_t req; // parsed request.
    config.LKHostConfig *hc; // resolved host config.
    int subroutine; // current running subroutine.
    io.handle_t *filehandle; // for serving a local file.

    exec.proc_t *cgiproc; // for serving a cgi script.
    io.buf_t *tmpbuf;
} ctx_t;

pub ctx_t *newctx(io.handle_t *h) {
    ctx_t *ctx = calloc(1, sizeof(ctx_t));
    ctx->client_handle = h;
    ctx->inbuf = io.newbuf();
    ctx->outbuf = io.newbuf();
    ctx->tmpbuf = io.newbuf();
    return ctx;
}

pub void freectx(ctx_t *ctx) {
    io.freebuf(ctx->inbuf);
    io.freebuf(ctx->outbuf);
    io.freebuf(ctx->tmpbuf);
    free(ctx);
}
