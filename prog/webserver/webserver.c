#import fs
#import http
#import opt
#import os/misc
#import os/net
#import server.c
#import srvcgi.c
#import strings
#import os/threads

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

	server.server_t SERVER = {};
	if (!misc.getcwd(SERVER.homedir, sizeof(SERVER.homedir))) {
		panic("failed to get current working directory: %s", strerror(errno));
	}
	strcpy(SERVER.cgidir, "/cgi-bin/");
	if (!fs.realpath("/cgi-bin/", SERVER.cgidir, sizeof(SERVER.cgidir))) {
		fprintf(stderr, "failed to resolve cgidir: %s\n", strerror(errno));
	}
	printf("homedir = %s\n", SERVER.homedir);
	printf("cgidir = %s\n", SERVER.cgidir);

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
		ctx_t *ctx = calloc(1, sizeof(ctx));
		ctx->s = &SERVER;
		ctx->conn = conn;
		if (!ctx) panic("calloc failed");
		threads.thr_new(client_routine, ctx);
	}
    panic("unreachable");
}

typedef {
	server.server_t *s;
	net.net_t *conn;
} ctx_t;

void *client_routine(void *arg) {
	ctx_t *ctx = arg;
	net.net_t *conn = ctx->conn;
	server.server_t *s = ctx->s;

	http.request_t req = {};
	while (true) {
		int err = http.read_request(&req, conn);
		if (err == EOF) {
			printf("failed to read request: EOF\n");
			break;
		}
		if (err) {
			panic("failed to read request: err = %d\n", err);
		}
		if (strlen(s->cgidir) > 0 && strings.starts_with(req.path, s->cgidir)) {
			printf("spawning a cgi subroutine\n");
			srvcgi.client_routine(s, &req, conn);
		} else {
			http.servefile(&req, conn, s->homedir);
		}
	}
	net.net_close(conn);
	return NULL;
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
