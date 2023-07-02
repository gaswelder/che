#import fs
#import http
#import ioroutine
#import os/exec
#import os/io
#import os/misc
#import strings

#import context.c
#import lkhostconfig.c
#import srvstd.c


enum {
    BEGIN,
    READ_PROC,
    WRITE_OUT,
    FLUSH
};

pub int client_routine(void *_ctx, int line) {
    context.ctx_t *ctx = _ctx;
    http.request_t *req = &ctx->req;
    lkhostconfig.LKHostConfig *hc = ctx->hc;

    switch (line) {
    case BEGIN:
        printf("resolving CGI\n");
        char *cgifile = strings.newstr("%s/%s", hc->homedir_abspath, req->path);
        printf("cgifile = %s\n", cgifile);
        // Expand "/../", etc. into real_path.
        char real_path[PATH_MAX];
        bool pathok = fs.realpath(cgifile, real_path, sizeof(real_path));
        free(cgifile);
        printf("realpath = %s\n", real_path);
        // real_path should start with cgidir_abspath
        // real_path file should exist
        if (!pathok
            || !strings.starts_with(real_path, hc->cgidir_abspath)
            || !fs.file_exists(real_path)
        ) {
            printf("invalid path\n");
            srvstd.write_404(req, ctx);
            return FLUSH;
        }
        printf("setting env\n");
        char hostname[1024] = {0};
        if (!misc.get_hostname(hostname, sizeof(hostname))) {
            fprintf(stderr, "failed to get hostname: %s\n", strerror(errno));
            exit(1);
        }
        printf("hostname = %s\n", hostname);

        printf("setting env\n");
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

        printf("starting\n");
        char *args[] = {path, NULL};
        ctx->cgiproc = exec.spawn(args, env);
        if (!ctx->cgiproc) {
            panic("spawn failed");
        }
        printf("spawned %d\n", ctx->cgiproc->pid);
        return READ_PROC;

    /*
     * Writes a portion of output from the child process into the output buffer.
     */
    case READ_PROC:
        if (!ioroutine.ioready(ctx->cgiproc->stdout, io.READ)) {
            return READ_PROC;
        }
        if (!io.read(ctx->cgiproc->stdout, ctx->outbuf)) {
            panic("read failed");
            return -1;
        }
        size_t n = io.bufsize(ctx->outbuf);
        printf("read from proc, have %zu\n", n);
        if (n == 0) {
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
        printf("writing %zu out\n", io.bufsize(ctx->outbuf));
        if (!io.write(ctx->client_handle, ctx->outbuf)) {
            panic("write failed: %s", strerror(errno));
            return -1;
        }
        printf("written out, have %zu\n", io.bufsize(ctx->outbuf));
        return READ_PROC;
    
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
