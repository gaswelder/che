#import fs
#import http
#import ioroutine
#import opt
#import os/io
#import os/misc
#import strings
#import strings

#import configparser.c
#import config.c
#import context.c
#import srvcgi.c
#import srvfiles.c

typedef {
    io.handle_t *listener;
    config.LKHostConfig *hostconfigs[256];
    size_t hostconfigs_size;
} server_t;

server_t SERVER = {};

int main(int argc, char *argv[]) {    
    char *port = "8000";
    char *host = "localhost";
    char *configfile = NULL;

    opt.opt_str("p", "port", &port);
    opt.opt_str("h", "host", &host);
    opt.opt_str("f", "configfile", &configfile);

    char **rest = opt.opt_parse(argc, argv);
    if (*rest) {
        fprintf(stderr, "too many arguments\n");
        exit(1);
    }

    if (configfile) {
        SERVER.hostconfigs_size = configparser.read_file(configfile, SERVER.hostconfigs);
        if (!SERVER.hostconfigs_size) {
            fprintf(stderr, "failed to parse config file %s: %s", configfile, strerror(errno));
            return 1;
        }
    } else {
        config.LKHostConfig *hc = config.lk_hostconfig_new("*");
        if (!hc) {
            panic("failed to create a host config");
        }
        if (!misc.getcwd(hc->homedir, sizeof(hc->homedir))) {
            panic("failed to get current working directory: %s", strerror(errno));
        }
        strcpy(hc->cgidir, "/cgi-bin/");
        SERVER.hostconfigs[SERVER.hostconfigs_size++] = hc;
    }

    // Set homedir absolute paths for hostconfigs.
    // Adjust /cgi-bin/ paths.
    for (size_t i = 0; i < SERVER.hostconfigs_size; i++) {
        config.LKHostConfig *hc = SERVER.hostconfigs[i];

        // Skip hostconfigs that don't have have homedir.
        if (strlen(hc->homedir) == 0) {
            continue;
        }

        if (!fs.realpath(hc->homedir, hc->homedir_abspath, sizeof(hc->homedir_abspath))) {
            panic("realpath failed");
        }

        if (strlen(hc->cgidir) > 0) {
            char *tmp = strings.newstr("%s/%s/", hc->homedir_abspath, hc->cgidir);
            if (!fs.realpath(tmp, hc->cgidir_abspath, sizeof(hc->cgidir_abspath))) {
                panic("realpath failed: %s", strerror(errno));
            }
            free(tmp);
        }
    }
    print_configs();

    // TODO
    // signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    // signal(SIGINT, handle_sigint);      // exit on CTRL-C
    // signal(SIGCHLD, handle_sigchld);


    char *addr = strings.newstr("%s:%s", host, port);
    SERVER.listener = io.listen("tcp", addr);
    if (!SERVER.listener) {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        return 1;
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
        if (c > 100) {
            panic("!");
        }
        ioroutine.step();
    }
    panic("unreachable");
}

int listener_routine(void *_ctx, int line) {
    server_t *s = _ctx;
    switch (line) {
    /*
     * The only state for this routine.
     * Waits for a new connection, accepts and spawns a new routine for
     * the new connection.
     */
    case 0:
        if (!ioroutine.ioready(s->listener, io.READ)) {
            return 0;
        }
        io.handle_t *conn = io.accept(s->listener);
        if (!conn) {
            panic("accept failed: %s\n", strerror(errno));
        }
        printf("accepted %d\n", conn->fd);
        context.ctx_t *ctx = context.newctx(conn);
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
    context.ctx_t *ctx = _ctx;
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
        ctx->hc = lk_config_find_hostconfig(&SERVER, hostname);
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

bool parse_request(context.ctx_t *ctx) {
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

// bool resolve_request(server_t *server, context.ctx_t *ctx) {

//     // Replace path with any matching alias.
//     char *match = stringtable.lk_stringtable_get(hc->aliases, req->path);
//     if (match != NULL) {
//         panic("todo");
//         if (strlen(match) + 1 > sizeof(req->path)) {
//             abort();
//         }
//         strcpy(req->path, match);
//     }
// }



// void handle_sigint(int sig) {
//     printf("SIGINT received: %d\n", sig);
//     fflush(stdout);
//     exit(0);
// }

// void handle_sigchld(int sig) {
//     printf("sigchild %d\n", sig);
//     abort();
//     // int tmp_errno = errno;
//     // while (waitpid(-1, NULL, WNOHANG) > 0) {}
//     // errno = tmp_errno;
// }

void print_configs() {
    for (size_t i = 0; i < SERVER.hostconfigs_size; i++) {
        config.print(SERVER.hostconfigs[i]);
    }
    printf("\n");
}

// Return hostconfig matching hostname,
// or if hostname parameter is NULL, return hostconfig matching "*".
// Return NULL if no matching host
config.LKHostConfig *lk_config_find_hostconfig(server_t *s, char *hostname) {
    if (hostname != NULL) {
        for (size_t i = 0; i < s->hostconfigs_size; i++) {
            config.LKHostConfig *hc = s->hostconfigs[i];
            if (!strcmp(hc->hostname, hostname)) {
                return hc;
            }
        }
    }
    // If hostname not found, return hostname * (fallthrough hostname).
    for (size_t i = 0; i < s->hostconfigs_size; i++) {
        config.LKHostConfig *hc = s->hostconfigs[i];
        if (!strcmp(hc->hostname, "*")) {
            return hc;
        }
    }
    return NULL;
}
