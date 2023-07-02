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
        misc.clearenv();
        misc.setenv("SERVER_NAME", hostname, 1);
        misc.setenv("SERVER_SOFTWARE", "littlekitten/0.1", 1);
        misc.setenv("SERVER_PROTOCOL", "HTTP/1.0", 1);
        misc.setenv("SERVER_PORT", "todo", 1);

        misc.setenv("DOCUMENT_ROOT", hc->homedir_abspath, 1);

        http.header_t *h = NULL;
        h = http.get_header(req, "User-Agent");
        if (h) {
            misc.setenv("HTTP_USER_AGENT", h->value, 1);
        } else {
            misc.setenv("HTTP_USER_AGENT", "", 1);
        }
        h = http.get_header(req, "Host");
        if (h) {
            misc.setenv("HTTP_HOST", h->value, 1);
        } else {
            misc.setenv("HTTP_HOST", "", 1);
        }

        h = http.get_header(req, "Content-Type");
        if (h) {
            misc.setenv("CONTENT_TYPE", h->value, 1);
        } else {
            misc.setenv("CONTENT_TYPE", "", 1);
        }
        char filename[3000] = {0};
        if (strlen(hc->homedir_abspath) + strlen(req->path) + 1 >= sizeof(filename)) {
            panic("filename buffer too short");
        }
        sprintf(filename, "%s/%s", hc->homedir_abspath, req->path);
        misc.setenv("SCRIPT_FILENAME", filename, 1);
        misc.setenv("REQUEST_METHOD", req->method, 1);
        misc.setenv("SCRIPT_NAME", req->path, 1);
        misc.setenv("REQUEST_URI", req->uri, 1);
        misc.setenv("QUERY_STRING", req->query, 1);
        misc.setenv("CONTENT_LENGTH", "todo", 1);
        misc.setenv("REMOTE_ADDR", "todo", 1);
        misc.setenv("REMOTE_PORT", "todo", 1);

        printf("starting\n");
        char *args[] = {real_path, NULL};
        char *env[] = {NULL};
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
