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
    config.LKConfig *cfg;
    io.handle_t *listener;
} server_t;

server_t SERVER = {};

int main(int argc, char *argv[]) {    
    char *cgidir = NULL;
    char *homedir = NULL;
    char *port = "8000";
    char *host = "localhost";
    char *configfile = NULL;

    opt.opt_str("p", "port", &port);
    opt.opt_str("h", "host", &host);
    opt.opt_str("d", "homedir", &homedir);
    opt.opt_str("f", "configfile", &configfile);
    opt.opt_str("d", "CGI directory", &cgidir);

    char **rest = opt.opt_parse(argc, argv);
    if (*rest) {
        fprintf(stderr, "too many arguments\n");
        exit(1);
    }

    // Create the config.
    config.LKConfig *cfg = NULL;

    if (configfile) {
        cfg = configparser.read_file(configfile);
        if (!cfg) {
            fprintf(stderr, "failed to parse config file %s: %s", configfile, strerror(errno));
            return 1;
        }
    } else {
        cfg = config.lk_config_new();
    }

    // TODO
    // signal(SIGPIPE, SIG_IGN);           // Don't abort on SIGPIPE
    // signal(SIGINT, handle_sigint);      // exit on CTRL-C
    // signal(SIGCHLD, handle_sigchld);

    if (cgidir) {
        config.LKHostConfig *default_hc = config.lk_config_create_get_hostconfig(cfg, "*");
        strcpy(default_hc->cgidir, cgidir);
    }
    
    if (homedir) {
        config.LKHostConfig *hc0 = config.lk_config_create_get_hostconfig(cfg, "*");
        strcpy(hc0->homedir, homedir);
        // cgidir default to cgi-bin
        if (strlen(hc0->cgidir) == 0) {
            strcpy(hc0->cgidir, "/cgi-bin/");
        }
    }

    // If no hostconfigs, add a fallthrough '*' hostconfig.
    if (cfg->hostconfigs_size == 0) {
        config.LKHostConfig *hc = config.lk_hostconfig_new("*");
        if (!hc) {
            panic("failed to create a host config");
        }
        if (!misc.getcwd(hc->homedir, sizeof(hc->homedir))) {
            panic("failed to get current working directory: %s", strerror(errno));
        }
        strcpy(hc->cgidir, "/cgi-bin/");
        config.add_hostconfig(cfg, hc);
    }

    // Set homedir absolute paths for hostconfigs.
    // Adjust /cgi-bin/ paths.
    for (size_t i = 0; i < cfg->hostconfigs_size; i++) {
        config.LKHostConfig *hc = cfg->hostconfigs[i];

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
    config.print(cfg);

    SERVER.cfg = cfg;

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
        ctx->hc = config.lk_config_find_hostconfig(SERVER.cfg, hostname);
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
