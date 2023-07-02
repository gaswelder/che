#import http
#import os/io
#import os/misc
#import strings

#import ioroutine.c

#import lkconfig.c
#import lkcontext.c
#import srvfiles.c
#import srvcgi.c
#import llist.c

pub typedef {
    lkconfig.LKConfig *cfg;
    llist.t *contexts;
    io.handle_t *listener;
} server_t;

server_t SERVER = {};

pub bool lk_httpserver_serve(lkconfig.LKConfig *cfg) {
    SERVER.cfg = cfg;
    SERVER.contexts = llist.new();

    /*
     * Get the hostname and set common env for future CGI children.
     */
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
    misc.setenv("SERVER_PORT", cfg->port, 1);

    /*
     * Start listening.
     */
    char *addr = strings.newstr("%s:%s", cfg->serverhost, cfg->port);
    SERVER.listener = io.listen("tcp", addr);
    if (!SERVER.listener) {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        free(addr);
        return false;
    }
    printf("listening at %d\n", SERVER.listener->fd);
    printf("Serving at http://%s\n", addr);
    free(addr);

    /*
     * Run the io routines.
     */
    ioroutine.init();
    ioroutine.spawn(listener_routine, &SERVER);
    int c = 0;
    while (true) {
        printf("-------------- loop %d --------------\n", c++);
        if (c > 10) {
            panic("!");
        }
        ioroutine.step();
    }
    panic("unreachable");
}

int listener_routine(void *ctx, int line) {
    server_t *s = (server_t *) ctx;
    switch (line) {
        case 0:
            if (!ioroutine.ioready(s->listener, io.READ)) {
                return 0;
            }
            printf("accepting\n");
            io.handle_t *conn = io.accept(s->listener);
            printf("accepted %d\n", conn->fd);
            if (!conn) {
                fprintf(stderr, "accept failed: %s\n", strerror(errno));
                panic("!");
            }
            lkcontext.LKContext *ctx = lkcontext.create_initial_context(conn);
            // llist.append(s->contexts, ctx);
            ioroutine.spawn(client_routine, ctx);
            return 0;
    }
    panic("unexpected line: %d", line);
}

enum {
    READ_REQUEST,
    RESOLVE_REQUEST,
    WAIT_SUBROUTINE
};

int client_routine(void *_ctx, int line) {
    lkcontext.LKContext *ctx = _ctx;
    switch (line) {
    /*
     * Read data from the socket until a full request head is parsed.
     */
    case READ_REQUEST:
        if (!ioroutine.ioready(ctx->client_handle, io.READ)) {
            return READ_REQUEST;
        }
        if (!io.read(ctx->client_handle, ctx->inbuf)) {
            panic("read failed");
        }
        if (!parse_request(ctx)) {
            return READ_REQUEST;
        }
        return RESOLVE_REQUEST;

    /*
     * Look at the request and spawn a corresponding routine to process it.
     */
    case RESOLVE_REQUEST:
        printf("getting a host config\n");
        char *hostname = NULL;
        http.header_t *host = http.get_header(&ctx->req, "Host");
        if (host) {
            hostname = host->value;
        }
        ctx->hc = lkconfig.lk_config_find_hostconfig(SERVER.cfg, hostname);
        if (!ctx->hc) {
            // 404?
            panic("failed to find the host config");
        }
        printf("cgidir = %s, path = %s\n", ctx->hc->cgidir, ctx->req.path);
        if (strlen(ctx->hc->cgidir) > 0 && strings.starts_with(ctx->req.path, ctx->hc->cgidir)) {
            printf("spawning a cgi subroutine\n");
            ctx->subroutine = ioroutine.spawn(srvcgi.client_routine, ctx);
            return WAIT_SUBROUTINE;
        }
        // Fall back to serving local files.
        printf("spawning a files subroutine\n");
        ctx->subroutine = ioroutine.spawn(srvfiles.client_routine, ctx);
        return WAIT_SUBROUTINE;

    /*
     * Wait for the subroutine to finish.
     */
    case WAIT_SUBROUTINE:
        if (!ioroutine.done(ctx->subroutine)) {
            return WAIT_SUBROUTINE;
        }
        printf("subroutine done\n");
        return READ_REQUEST;
    }

    panic("unhandled state: %d", line);
}

bool parse_request(lkcontext.LKContext *ctx) {
    printf("read from %d, have %zu\n", ctx->client_handle->fd, io.bufsize(ctx->inbuf));
    // See if the input buffer has a finished request header yet.
    char *split = strstr(ctx->inbuf->data, "\r\n\r\n");
    if (!split || (size_t)(split - ctx->inbuf->data) > ctx->inbuf->size) {
        return false;
    }
    split += 4;

    // Extract the headers and shift the buffer.
    char head[4096] = {0};
    size_t headlen = split - ctx->inbuf->data;
    memcpy(head, ctx->inbuf->data, headlen);
    io.shift(ctx->inbuf, headlen);
    // fprintf(stderr, "got a request:\n=============\n%s\n=============\n", head);

    // Parse the request.
    if (!http.parse_request(&ctx->req, head)) {
        panic("failed to parse the request");
    }
    return true;
}

// bool resolve_request(server_t *server, lkcontext.LKContext *ctx) {

//     // Replace path with any matching alias.
//     char *match = lkstringtable.lk_stringtable_get(hc->aliases, req->path);
//     if (match != NULL) {
//         panic("todo");
//         if (strlen(match) + 1 > sizeof(req->path)) {
//             abort();
//         }
//         strcpy(req->path, match);
//     }
// }

