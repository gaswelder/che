#import clip/stringtable
#import http
#import os/exec
#import os/io

pub typedef {
    io.handle_t *listener;
    hostconfig_t *hostconfigs[256];
    size_t hostconfigs_size;
} server_t;

pub typedef {
    char hostname[1000];
    char homedir[1000];
    char homedir_abspath[1000];
    char cgidir[1000];
    char cgidir_abspath[1000];
    char proxyhost[1000];

    stringtable.t *aliases;
} hostconfig_t;

pub hostconfig_t *lk_hostconfig_new(char *hostname) {
    hostconfig_t *hc = calloc(1, sizeof(hostconfig_t));
    hc->aliases = stringtable.new();
    strcpy(hc->hostname, hostname);
    return hc;
}

pub typedef {
    io.handle_t *client_handle; // Connected client.
    io.buf_t *inbuf, *outbuf; // Buffers for incoming and outgoing data.
    http.request_t req; // parsed request.
    hostconfig_t *hc; // resolved host config.
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

pub void print_config(hostconfig_t *hc) {
    printf("--- vhost ---\n");
    printf("hostname = %s\n", hc->hostname);
    if (strlen(hc->homedir) > 0) {
        printf("homedir = %s\n", hc->homedir);
    }
    if (strlen(hc->cgidir) > 0) {
        printf("cgidir = %s (%s)\n", hc->cgidir, hc->cgidir_abspath);
    }
    if (strlen(hc->proxyhost) > 0) {
        printf("proxyhost = %s\n", hc->proxyhost);
    }
    for (size_t j = 0; j < hc->aliases->len; j++) {
        printf("    alias %s=%s\n", hc->aliases->items[j].k, hc->aliases->items[j].v);
    }
    printf("------\n");
}

// Return hostconfig matching hostname,
// or if hostname parameter is NULL, return hostconfig matching "*".
// Return NULL if no matching host
pub hostconfig_t *lk_config_find_hostconfig(server_t *s, char *hostname) {
    if (hostname != NULL) {
        for (size_t i = 0; i < s->hostconfigs_size; i++) {
            hostconfig_t *hc = s->hostconfigs[i];
            if (!strcmp(hc->hostname, hostname)) {
                return hc;
            }
        }
    }
    // If hostname not found, return hostname * (fallthrough hostname).
    for (size_t i = 0; i < s->hostconfigs_size; i++) {
        hostconfig_t *hc = s->hostconfigs[i];
        if (!strcmp(hc->hostname, "*")) {
            return hc;
        }
    }
    return NULL;
}
