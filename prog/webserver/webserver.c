#import formats/json
#import opt
#import os/fs
#import os/net
#import os/self
#import os/threads
#import protocols/http
#import reader
#import srvcgi.c
#import strbuilder
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
		log_error("Failed to listen at %s: %s", addr, strerror(errno));
        return 1;
    }
    log_info("Serving %s at http://%s", SERVER.homedir, addr);

	while (true) {
		net.net_t *conn = net.net_accept(ln);
		if (!conn) panic("accept failed");

		log_info("%s connected", net.net_addr(conn));

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
			panic("failed to read request: %s", strerror(errno));
		}
		log_info("%s %s", req.method, req.path);

		if (strcmp(req.path, "/") == 0) {
			strbuilder.str *b = strbuilder.new();
			strbuilder.adds(b, "<a href=/>home</a><br><br>");
			fs.dir_t *d = fs.dir_open(".");
			while (true) {
				const char *name = fs.dir_next(d);
				if (!name) break;
				strbuilder.addf(b, "<a href=\"%s\">%s</a><br>", name, name);
			}
			fs.dir_close(d);
			http.serve_text(&req, conn, strbuilder.str_raw(b), "text/html");
			strbuilder.str_free(b);
			continue;
		}

		char *filepath = resolve_path(s->homedir, req.path);
		if (!filepath) {
			log_info("404 \"%s\" was not found", req.path);
			http.write_404(&req, conn);
			continue;
		}
		log_info("%s is %s", req.path, filepath);

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

void handle_sigint(int sig) {
    printf("SIGINT received: %d\n", sig);
	fflush(stdout);
    exit(0);
}

char *resolve_path(const char *homedir, *reqpath) {
    char naive_path[4096] = {};
    if (strlen(homedir) + strlen(reqpath) + 1 >= sizeof(naive_path)) {
		log_error("path is too long: %s", reqpath);
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

void log_info(const char *f, ...) {
	char buf[4096] = {};

	va_list args = {};
	va_start(args, f);
	vsnprintf(buf, sizeof(buf)-1, f, args);
	va_end(args);

	printf("{\"level\":\"info\",\"msg\":");
	json.write_string(stdout, buf);
	printf("}\n");
	fflush(stdout);
}

void log_error(const char *f, ...) {
	char buf[4096] = {};

	va_list args = {};
	va_start(args, f);
	vsnprintf(buf, sizeof(buf)-1, f, args);
	va_end(args);

	printf("{\"level\":\"error\",\"msg\":");
	json.write_string(stdout, buf);
	printf("}\n");
	fflush(stdout);
}
