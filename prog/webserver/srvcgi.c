#import os/net
#import os/proc
#import os/self
#import protocols/cgi
#import protocols/http
#import reader
#import ascii
#import fmt
#import writer

pub void cgi(char *path, http.request_t *req, net.conn_t *conn) {
	proc.proc_t *proc = start(path, req);

	// Read the script's output.
	char output[4096] = {};
	int r = reader.read(proc->stdout, (uint8_t *) output, 4096);
	if (r < 0) {
		panic("read failed");
	}
	size_t output_size = (size_t) r;

	int status = 0;
	proc.wait(proc, &status);
	if (status != 0) {
		panic("process exited with %d\n", status);
	}

	// Parse the output.
	cgi.head_t head = {};
	size_t headsize = cgi.parse_head(&head, output, (size_t) r);
	if (!headsize) {
		panic("failed to parse cgi head");
	}

	writer.t *w = net.getwriter(conn);

	fmt.write(w, "%s 200 OK\n", req->version);
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
		fmt.write(w, "%s: %s\n", name, value);
	}
	fmt.write(w, "Content-Length: %zu\r\n", output_size - headsize);
	fmt.write(w, "\r\n", 1);
	net.write(conn, output + headsize, output_size - headsize);
	writer.free(w);
}

proc.proc_t *start(char *path, http.request_t *req) {
	char hostname[1024] = {};
	if (!self.gethostname(hostname, sizeof(hostname))) {
		panic("failed to get hostname: %s", strerror(errno));
	}

	char *env[100] = {};
	char **p = env;
	*p++ = ascii.newstr("SERVER_NAME=%s", hostname);
	*p++ = ascii.newstr("SERVER_PORT", "todo", 1);
	*p++ = ascii.newstr("SERVER_SOFTWARE=webserver");
	*p++ = ascii.newstr("SERVER_PROTOCOL=HTTP/1.0");
	*p++ = ascii.newstr("DOCUMENT_ROOT=%s", "todo");
	*p++ = ascii.newstr("HTTP_USER_AGENT=%s", header(req, "User-Agent", ""));
	*p++ = ascii.newstr("HTTP_HOST=%s", header(req, "Host", ""));
	*p++ = ascii.newstr("CONTENT_TYPE=%s", header(req, "Content-Type", ""));
	*p++ = ascii.newstr("SCRIPT_FILENAME=%s", path);
	*p++ = ascii.newstr("REQUEST_METHOD=%s", req->method);
	*p++ = ascii.newstr("SCRIPT_NAME=%s", req->path);
	*p++ = ascii.newstr("REQUEST_URI=%s", req->uri);
	*p++ = ascii.newstr("QUERY_STRING=%s", req->query);
	*p++ = ascii.newstr("CONTENT_LENGTH=%s", "todo");
	*p++ = ascii.newstr("REMOTE_ADDR=%s", "todo");
	*p++ = ascii.newstr("REMOTE_PORT=%s", "todo");

	char *args[] = { path, NULL };
	proc.proc_t *proc = proc.spawn(args, env);
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
