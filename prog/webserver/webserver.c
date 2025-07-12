#import opt
#import os/fs
#import os/net
#import os/self
#import os/threads
#import protocols/http
#import reader
#import srvcgi.c
#import strings

typedef {
    char homedir[1000];
} server_t;

typedef {
	server_t *s;
	net.net_t *conn;
} ctx_t;


int main(int argc, char *argv[]) {
	char *addr = "localhost:8000";
    opt.str("a", "listen address", &addr);
	opt.nargs(0, "");
	opt.parse(argc, argv);

	signal(SIGINT, handle_sigint);
	server_t SERVER = {};
	if (!self.getcwd(SERVER.homedir, sizeof(SERVER.homedir))) {
		panic("failed to get current working directory: %s", strerror(errno));
	}
	net.net_t *ln = net.net_listen("tcp", addr);
    if (!ln) {
        fprintf(stderr, "Failed to listen at %s: %s\n", addr, strerror(errno));
        return 1;
    }
    printf("Serving %s at http://%s\n", SERVER.homedir, addr);

	while (true) {
		net.net_t *conn = net.net_accept(ln);
		if (!conn) panic("accept failed");
		printf("%s connected\n", net.net_addr(conn));
		ctx_t *ctx = calloc!(1, sizeof(ctx));
		ctx->s = &SERVER;
		ctx->conn = conn;
		threads.start(client_routine, ctx);
	}
    panic("unreachable");
}

void *client_routine(void *arg) {
	ctx_t *ctx = arg;
	net.net_t *conn = ctx->conn;
	reader.t *re = net.getreader(conn);
	server_t *s = ctx->s;

	http.request_t req = {};
	while (true) {
		if (!http.read_request(re, &req)) {
			panic("failed to read request");
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
	reader.free(re);
	net.close(conn);
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
