#import fs
#import http
#import opt
#import os/os
#import os/net
#import server.c
#import srvcgi.c
#import strings
#import os/threads

int main(int argc, char *argv[]) {
    char *port = "8000";
    char *host = "localhost";
    opt.str("p", "port", &port);
    opt.str("h", "host", &host);
	opt.nargs(0, "");

    char **rest = opt.parse(argc, argv);
    if (*rest) {
        fprintf(stderr, "too many arguments\n");
        exit(1);
    }

	server.server_t SERVER = {};
	if (!os.getcwd(SERVER.homedir, sizeof(SERVER.homedir))) {
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

		printf("resolving %s %s\n", req.method, req.path);
		char *filepath = resolve_path(s->homedir, req.path);
		if (!filepath) {
			printf("file \"%s\" not found\n", req.path);
			http.write_404(&req, conn);
			continue;
		}
		printf("resolved %s as %s\n", req.path, filepath);
		if (strings.starts_with(req.path, "/cgi-bin/")) {
			srvcgi.cgi(filepath, &req, conn);
		} else {
			http.servefile(&req, conn, filepath);
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

const char *index_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "default.htm"
};

char *resolve_path(const char *homedir, *reqpath) {
    if (!strcmp(reqpath, "/")) {
        for (size_t i = 0; i < nelem(index_files); i++) {
            char *p = resolve_inner(homedir, index_files[i]);
            if (p) {
                return p;
            }
        }
        return NULL;
    }
    return resolve_inner(homedir, reqpath);
}

char *resolve_inner(const char *homedir, *reqpath) {
    char naive_path[4096] = {0};
    if (strlen(homedir) + strlen(reqpath) + 1 >= sizeof(naive_path)) {
        printf("ERROR: requested path is too long: %s\n", reqpath);
        return NULL;
    }
    sprintf(naive_path, "%s/%s", homedir, reqpath);

    char realpath[4096] = {0};
    if (!fs.realpath(naive_path, realpath, sizeof(realpath))) {
        return NULL;
    }
    if (!strings.starts_with(realpath, homedir)) {
        printf("resolved path \"%s\" doesn't start with homedir=\"%s\"\n", realpath, homedir);
        return NULL;
    }
    return strings.newstr("%s", realpath);
}
