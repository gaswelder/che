#import os/io
#import os/exec
#import http

#import lkhostconfig.c

pub typedef {
    io.handle_t *client_handle; // Connected client.
    io.buf_t *inbuf, *outbuf; // Buffers for incoming and outgoing data.
    http.request_t req; // parsed request.
    lkhostconfig.LKHostConfig *hc; // resolved host config.
    int subroutine; // current running subroutine.
    io.handle_t *filehandle; // for serving a local file.
    exec.proc_t *cgiproc; // for serving a cgi script.
} ctx_t;

pub ctx_t *newctx(io.handle_t *h) {
    ctx_t *ctx = calloc(1, sizeof(ctx_t));
    ctx->client_handle = h;
    ctx->inbuf = io.newbuf();
    ctx->outbuf = io.newbuf();
    return ctx;
}

pub void freectx(ctx_t *ctx) {
    io.freebuf(ctx->inbuf);
    io.freebuf(ctx->outbuf);
    free(ctx);
}
