#import fs
#import http
#import protocols/cgi
#import os/exec
#import os/misc
#import strings
#import os/net
#import server.c

pub int client_routine(server.server_t *server, http.request_t *req, net.net_t *conn) {
	printf("resolving CGI script path\n");
	char path[1000] = {0};
	if (!resolve_path(server, req, path, sizeof(path))) {
		printf("invalid path: %s\n", req->path);
		http.write_404(req, conn);
		return 123;
	}

	char hostname[1024] = {0};
	if (!misc.get_hostname(hostname, sizeof(hostname))) {
		panic("failed to get hostname: %s", strerror(errno));
	}

	char *env[100] = {NULL};
	char **p = env;
	*p++ = strings.newstr("SERVER_NAME=%s", hostname);
	*p++ = strings.newstr("SERVER_PORT", "todo", 1);
	*p++ = strings.newstr("SERVER_SOFTWARE=webserver");
	*p++ = strings.newstr("SERVER_PROTOCOL=HTTP/1.0");
	*p++ = strings.newstr("DOCUMENT_ROOT=%s", server->homedir);
	*p++ = strings.newstr("HTTP_USER_AGENT=%s", header(req, "User-Agent", ""));
	*p++ = strings.newstr("HTTP_HOST=%s", header(req, "Host", ""));
	*p++ = strings.newstr("CONTENT_TYPE=%s", header(req, "Content-Type", ""));
	*p++ = strings.newstr("SCRIPT_FILENAME=%s/%s", server->homedir, req->path);
	*p++ = strings.newstr("REQUEST_METHOD=%s", req->method);
	*p++ = strings.newstr("SCRIPT_NAME=%s", req->path);
	*p++ = strings.newstr("REQUEST_URI=%s", req->uri);
	*p++ = strings.newstr("QUERY_STRING=%s", req->query);
	*p++ = strings.newstr("CONTENT_LENGTH=%s", "todo");
	*p++ = strings.newstr("REMOTE_ADDR=%s", "todo");
	*p++ = strings.newstr("REMOTE_PORT=%s", "todo");

	char *args[] = {path, NULL};
	exec.proc_t *proc = exec.spawn(args, env);
	if (!proc) {
		panic("spawn failed");
	}
	printf("spawned process %d\n", proc->pid);
	p = env;
	while (*p) {
		free(*p);
		p++;
	}

	
	// if (!io.read(proc->stdout, ctx->tmpbuf)) {
	// 	panic("read failed");
	// 	return -1;
	// }
	// Try to parse the CGI headers in the buffer.
	// If not, wait for more data.
	cgi.head_t head = {};
	char tmpbuf[4096] = {};
	size_t headsize = cgi.parse_head(&head, tmpbuf, 4096);
	if (!headsize) {
		return 123;
	}
	// io.shift(ctx->tmpbuf, headsize);

	// Write the output headers.
	if (!fprintf(stdout, "%s 200 OK\n", req->version)) {
		panic("!");
	}
	if (!fprintf(stdout, "Transfer-Encoding: chunked\n")) {
		panic("!");
	}
	for (size_t i = 0; i < head.nheaders; i++) {
		char *name = head.headers[i].name;
		char *value = head.headers[i].value;
		printf("%s = %s\n", name, value);
		if (!strcmp(name, "Status")) {
			if (strcmp(value, "200")) {
				panic("expected status '200', got '%s'", value);
			}
			continue;
		}
		if (!strcmp(name, "Content-Length")) {
			panic("omitting content length");
		}
		if (!fprintf(stderr, "%s: %s\n", name, value)) {
			panic("!");
		}
	}
	if (!fprintf(stderr, "\n")) {
		panic("!");
	}

	// if (!io.read(proc->stdout, ctx->tmpbuf)) {
	// 	panic("read failed");
	// 	return -1;
	// }
	// size_t n = io.bufsize(ctx->tmpbuf);
	size_t n = 123;

	// Move the chunk from tmpbuf to outbuf.
	if (!fprintf(stderr, "%lx\r\n", n)) {
		panic("!");
	}
	// if (!io.push(ctx->outbuf, ctx->tmpbuf->data, ctx->tmpbuf->size)) {
	// 	panic("!");
	// }
	if (!fprintf(stderr, "\r\n")) {
		panic("!");
	}
	// io.shift(ctx->tmpbuf, n);

	if (n == 0) {
		if (!fprintf(stderr, "\r\n")) {
			panic("!");
		}
		// The process has exited, see if there's anything in stderr.
		char buf[4096] = {0};
		size_t rr = OS.read(proc->stderr->fd, buf, 4096);
		printf("rr = %zu\n", rr);
		printf("%s\n", buf);
		printf("waiting\n");
		int status = 0;
		exec.wait(proc, &status);
		printf("process exited with %d\n", status);
		proc = NULL;
		return 123;
	}
	return 123;
}

bool resolve_path(server.server_t *hc, http.request_t *req, char *path, size_t n) {
    char *naivepath = strings.newstr("%s/%s", hc->homedir, req->path);
    printf("naivepath = %s\n", naivepath);
    bool pathok = fs.realpath(naivepath, path, n);
    free(naivepath);
    printf("realpath = %s\n", path);
    return pathok && strings.starts_with(path, hc->cgidir);
}

const char *header(http.request_t *req, const char *name, *def) {
    http.header_t *h = http.get_header(req, name);
    if (h) {
        return h->value;
    }
    return def;
}