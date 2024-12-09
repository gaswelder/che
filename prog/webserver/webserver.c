#import configparser.c
#import fs
#import http
#import opt
#import os/misc
#import os/net
#import server.c
#import srvcgi.c
#import srvfiles.c
#import strings
#import os/threads

server.server_t SERVER = {};

int main(int argc, char *argv[]) {
    char *port = "8000";
    char *host = "localhost";
    char *configfile = NULL;

    opt.str("p", "port", &port);
    opt.str("h", "host", &host);
    opt.str("f", "configfile", &configfile);

    char **rest = opt.parse(argc, argv);
    if (*rest) {
        fprintf(stderr, "too many arguments\n");
        exit(1);
    }

    if (configfile) {
        SERVER.hostconfigs_size = configparser.read_file(configfile, SERVER.hostconfigs);
        if (!SERVER.hostconfigs_size) {
            fprintf(stderr, "failed to parse server file %s: %s", configfile, strerror(errno));
            return 1;
        }
    } else {
        server.hostconfig_t *hc = server.lk_hostconfig_new("*");
        if (!hc) {
            panic("failed to create a host server");
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
        server.hostconfig_t *hc = SERVER.hostconfigs[i];
        char tmp[1000] = {0};
        if (strlen(hc->homedir) > 0) {
            if (fs.realpath(hc->homedir, tmp, sizeof(tmp))) {
                strcpy(hc->homedir, tmp);
            } else {
                fprintf(stderr, "failed to resolve homedir '%s': %s\n", hc->homedir, strerror(errno));
                hc->homedir[0] = '\0';
            }
        }
        if (strlen(hc->cgidir) > 0) {
            if (fs.realpath(hc->cgidir, tmp, sizeof(tmp))) {
                strcpy(hc->cgidir, tmp);
            } else {
                fprintf(stderr, "failed to resolve cgidir '%s': %s\n", hc->cgidir, strerror(errno));
                hc->cgidir[0] = '\0';
            }
        }
    }
    for (size_t i = 0; i < SERVER.hostconfigs_size; i++) {
        server.print_config(SERVER.hostconfigs[i]);
    }
    printf("\n");

    signal(SIGINT, handle_sigint);

    char *addr = strings.newstr("%s:%s", host, port);
	net.net_t *ln = net.net_listen("tcp", addr);
    if (!ln) {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        return 1;
    }
    printf("Serving at http://%s\n", addr);
    free(addr);

	while (true) {
		net.net_t *conn = net.net_accept(ln);
		if (!conn) panic("accept failed");
		server.ctx_t *ctx = server.newctx(conn);
		threads.thr_new(client_routine, ctx);
	}
    panic("unreachable");
}

void *client_routine(void *_ctx) {
    server.ctx_t *ctx = _ctx;
	while (true) {
		// read request
		char buf[1000] = {};
		size_t size = 1000;
		net.net_t *conn = ctx->conn;
		int r = net.readconn(conn, buf, size);
		if (r < 0) {
			panic("read failed");
		}
		if (!http.parse_request(&ctx->req, buf)) {
			panic("failed to parse request");
		}
		http.header_t *host = http.get_header(&ctx->req, "Host");
		if (host) {
			ctx->hc = server.find_hostconfig(&SERVER, host->value);
		}
		// If host is not specified of there's no config for the host,
		// fall back to the "*" config.
		if (!ctx->hc) {
			ctx->hc = server.find_hostconfig(&SERVER, "*");
			if (!ctx->hc) {
				panic("failed to get the fallback host config");
			}
		}
		printf("cgi? %s\n", ctx->hc->cgidir);
		if (strlen(ctx->hc->cgidir) > 0 && strings.starts_with(ctx->req.path, ctx->hc->cgidir)) {
			printf("spawning a cgi subroutine\n");
			srvcgi.client_routine(ctx);
		} else {
			srvfiles.serve(ctx);
		}
	}
}


// bool resolve_request(server_t *server, server.ctx_t *ctx) {

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

void handle_sigint(int sig) {
    printf("SIGINT received: %d\n", sig);
    fflush(stdout);
    exit(0);
}
