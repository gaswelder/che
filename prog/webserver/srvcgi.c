#import fs
#import http
#import ioroutine
#import os/exec
#import os/io
#import os/misc
#import strings

#import context.c
#import config.c
#import srvstd.c

enum {
    BEGIN,
    READ_CGI_HEAD,
    READ_CGI_BODY,
    WRITE_OUT,
    FLUSH
};

pub int client_routine(void *_ctx, int line) {
    context.ctx_t *ctx = _ctx;
    http.request_t *req = &ctx->req;
    config.LKHostConfig *hc = ctx->hc;

    switch (line) {
    case BEGIN:
        printf("resolving CGI script path\n");
        char path[1000] = {0};
        if (!resolve_path(hc, req, path, sizeof(path))) {
            printf("invalid path: %s\n", req->path);
            srvstd.write_404(req, ctx);
            return FLUSH;
        }

        char hostname[1024] = {0};
        if (!misc.get_hostname(hostname, sizeof(hostname))) {
            panic("failed to get hostname: %s", strerror(errno));
        }

        char *env[100] = {NULL};
        char **p = env;
        *p++ = strings.newstr("SERVER_NAME=%s", hostname);
        *p++ = strings.newstr("SERVER_PORT", "todo", 1);
        *p++ = strings.newstr("SERVER_SOFTWARE=webserver");
        *p++ = strings.newstr("SERVER_PROTOCOL=HTTP/1.0");
        *p++ = strings.newstr("DOCUMENT_ROOT=%s", hc->homedir_abspath);
        *p++ = strings.newstr("HTTP_USER_AGENT=%s", header(req, "User-Agent", ""));
        *p++ = strings.newstr("HTTP_HOST=%s", header(req, "Host", ""));
        *p++ = strings.newstr("CONTENT_TYPE=%s", header(req, "Content-Type", ""));
        *p++ = strings.newstr("SCRIPT_FILENAME=%s/%s", hc->homedir_abspath, req->path);
        *p++ = strings.newstr("REQUEST_METHOD=%s", req->method);
        *p++ = strings.newstr("SCRIPT_NAME=%s", req->path);
        *p++ = strings.newstr("REQUEST_URI=%s", req->uri);
        *p++ = strings.newstr("QUERY_STRING=%s", req->query);
        *p++ = strings.newstr("CONTENT_LENGTH=%s", "todo");
        *p++ = strings.newstr("REMOTE_ADDR=%s", "todo");
        *p++ = strings.newstr("REMOTE_PORT=%s", "todo");

        char *args[] = {path, NULL};
        ctx->cgiproc = exec.spawn(args, env);
        if (!ctx->cgiproc) {
            panic("spawn failed");
        }
        printf("spawned process %d\n", ctx->cgiproc->pid);
        p = env;
        while (*p) {
            free(*p);
            p++;
        }
        return READ_CGI_HEAD;

    /*
     * Reads the child process until a CGI head is parsed.
     */
    case READ_CGI_HEAD:
        if (!ioroutine.ioready(ctx->cgiproc->stdout, io.READ)) {
            return READ_CGI_HEAD;
        }
        if (!io.read(ctx->cgiproc->stdout, ctx->tmpbuf)) {
            panic("read failed");
            return -1;
        }
        // Try to parse the CGI headers in the buffer.
        // If not, wait for more data.
        http.cgi_head_t head = {};
        size_t headsize = http.parse_cgi_head(&head, ctx->tmpbuf->data, ctx->tmpbuf->size);
        if (!headsize) {
            return READ_CGI_HEAD;
        }
        io.shift(ctx->tmpbuf, headsize);

        // Write the output headers.
        if (!io.pushf(ctx->outbuf, "%s 200 OK\n", req->version)) {
            panic("!");
        }
        if (!io.pushf(ctx->outbuf, "Transfer-Encoding: chunked\n")) {
            panic("!");
        }
        for (size_t i = 0; i < head.nheaders; i++) {
            char *name = head.headers[i].name;
            char *value = head.headers[i].value;
            printf("%s = %s\n", name, value);
            if (!strcmp(name, "Status")) {
                if (strcmp(value, "200")) {
                    panic("expected status '200', got '%s'", value);
                }
                continue;
            }
            if (!strcmp(name, "Content-Length")) {
                panic("omitting content length");
            }
            if (!io.pushf(ctx->outbuf, "%s: %s\n", name, value)) {
                panic("!");
            }
        }
        if (!io.pushf(ctx->outbuf, "\n")) {
            panic("!");
        }
        return READ_CGI_BODY;

    /*
     * Writes a portion of output from the child process into the output buffer.
     */
    case READ_CGI_BODY:
        if (!ioroutine.ioready(ctx->cgiproc->stdout, io.READ)) {
            return READ_CGI_BODY;
        }
        if (!io.read(ctx->cgiproc->stdout, ctx->tmpbuf)) {
            panic("read failed");
            return -1;
        }
        size_t n = io.bufsize(ctx->tmpbuf);

        // Move the chunk from tmpbuf to outbuf.
        if (!io.pushf(ctx->outbuf, "%lx\r\n", n)) {
            panic("!");
        }
        if (!io.push(ctx->outbuf, ctx->tmpbuf->data, ctx->tmpbuf->size)) {
            panic("!");
        }
        if (!io.pushf(ctx->outbuf, "\r\n")) {
            panic("!");
        }
        io.shift(ctx->tmpbuf, n);

        if (n == 0) {
            if (!io.pushf(ctx->outbuf, "\r\n")) {
                panic("!");
            }
            // The process has exited, see if there's anything in stderr.
            char buf[4096] = {0};
            size_t rr = OS.read(ctx->cgiproc->stderr->fd, buf, 4096);
            printf("rr = %zu\n", rr);
            printf("%s\n", buf);
            printf("waiting\n");
            int status = 0;
            exec.wait(ctx->cgiproc, &status);
            printf("process exited with %d\n", status);
            ctx->cgiproc = NULL;
            return FLUSH;
        }
        return WRITE_OUT;

    /*
     * Writes output buffer to the client.
     */
    case WRITE_OUT:
        if (!ioroutine.ioready(ctx->client_handle, io.WRITE)) {
            return WRITE_OUT;
        }
        if (!io.write(ctx->client_handle, ctx->outbuf)) {
            panic("write failed: %s", strerror(errno));
            return -1;
        }
        return READ_CGI_BODY;

    /*
     * Write all there is in the output buffer and exit.
     */
    case FLUSH:
        if (io.bufsize(ctx->outbuf) == 0) {
            printf("nothing to flush, exit\n");
            return -1;
        }
        if (!ioroutine.ioready(ctx->client_handle, io.WRITE)) {
            return FLUSH;
        }
        if (!io.write(ctx->client_handle, ctx->outbuf)) {
            panic("write failed: %s", strerror(errno));
        }
        return FLUSH;
    }

    panic("unhandled state: %d", line);
}

bool resolve_path(config.LKHostConfig *hc, http.request_t *req, char *path, size_t n) {
    char *naivepath = strings.newstr("%s/%s", hc->homedir_abspath, req->path);
    printf("naivepath = %s\n", naivepath);
    bool pathok = fs.realpath(naivepath, path, n);
    free(naivepath);
    printf("realpath = %s\n", path);
    return pathok && strings.starts_with(path, hc->cgidir_abspath);
}

const char *header(http.request_t *req, const char *name, *def) {
    http.header_t *h = http.get_header(req, name);
    if (h) {
        return h->value;
    }
    return def;
}