#import http
#import protocols/cgi
#import os/exec
#import os/misc
#import strings
#import os/net
#import reader

pub void cgi(char *path, http.request_t *req, net.net_t *conn) {
	exec.proc_t *proc = start(path, req);

	// Read the script's output.
	char output[4096] = {};
	int r = reader.read(proc->stdout, (uint8_t*)output, 4096);
	if (r < 0) {
		panic("read failed");
	}
	size_t output_size = (size_t)r;

	int status = 0;
	exec.wait(proc, &status);
	if (status != 0) {
		panic("process exited with %d\n", status);
	}

	// Parse the output.
	cgi.head_t head = {};
	size_t headsize = cgi.parse_head(&head, output, (size_t) r);
	if (!headsize) {
		panic("failed to parse cgi head");
	}

	net.net_printf(conn, "%s 200 OK\n", req->version);
	for (size_t i = 0; i < head.nheaders; i++) {
		char *name = head.headers[i].name;
		char *value = head.headers[i].value;
		if (!strcmp(name, "Status")) {
			if (strcmp(value, "200")) {
				panic("expected status '200', got '%s'", value);
			}
			continue;
		}
		if (!strcmp(name, "Content-Length")) {
			panic("omitting content length");
		}
		net.net_printf(conn, "%s: %s\n", name, value);
	}
	net.net_printf(conn, "Content-Length: %zu\r\n", output_size - headsize);
	net.net_printf(conn, "\r\n", 1);
	net.net_write(conn, output + headsize, output_size - headsize);
}

exec.proc_t *start(char *path, http.request_t *req) {
	char hostname[1024] = {0};
	if (!misc.get_hostname(hostname, sizeof(hostname))) {
		panic("failed to get hostname: %s", strerror(errno));
	}

	char *env[100] = {};
	char **p = env;
	*p++ = strings.newstr("SERVER_NAME=%s", hostname);
	*p++ = strings.newstr("SERVER_PORT", "todo", 1);
	*p++ = strings.newstr("SERVER_SOFTWARE=webserver");
	*p++ = strings.newstr("SERVER_PROTOCOL=HTTP/1.0");
	*p++ = strings.newstr("DOCUMENT_ROOT=%s", "todo");
	*p++ = strings.newstr("HTTP_USER_AGENT=%s", header(req, "User-Agent", ""));
	*p++ = strings.newstr("HTTP_HOST=%s", header(req, "Host", ""));
	*p++ = strings.newstr("CONTENT_TYPE=%s", header(req, "Content-Type", ""));
	*p++ = strings.newstr("SCRIPT_FILENAME=%s", path);
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
	return proc;
}

const char *header(http.request_t *req, const char *name, *def) {
    http.header_t *h = http.get_header(req, name);
    if (h) {
        return h->value;
    }
    return def;
}
