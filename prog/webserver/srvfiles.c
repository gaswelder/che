#import fs
#import http
#import mime
#import os/net
#import server.c
#import strings

const char *default_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "default.htm"
};

pub void serve(http.request_t *req, net.net_t *conn, server.hostconfig_t *hc) {
    printf("resolving %s %s\n", req->method, req->path);
	char *filepath = resolve_path(hc->homedir, req->path);
	if (!filepath) {
		printf("file \"%s\" not found\n", req->path);
		http.write_404(req, conn);
		return;
	}
	printf("resolved %s as %s\n", req->path, filepath);
	const char *ext = fs.fileext(filepath);
	const char *content_type = mime.lookup(ext);
	if (content_type == NULL) {
		content_type = "text/plain";
	}
	size_t filesize = 0;
	if (!fs.filesize(filepath, &filesize)) {
		panic("failed to get file size");
	}
	char buf[1000] = {};
	sprintf(buf, "%s 200 OK\n"
		"Content-Length: %ld\n"
		"Content-Type: %s\n"
		"\n",
		req->version,
		filesize,
		content_type);
	net.net_write(conn, buf, strlen(buf));

	FILE *f = fopen(filepath, "rb");
	if (!f) {
		panic("oops");
	}

	while (true) {
		char tmp[4096];
		size_t n = fread(tmp, 1, 4096, f);
		printf("read %zu from file\n", n);
		if (n == 0) break;
		int r = net.net_write(conn, tmp, n);
		printf("wrote %d\n", r);
	}
	fclose(f);
}

char *resolve_path(const char *homedir, *reqpath) {
    if (!strcmp(reqpath, "/")) {
        for (size_t i = 0; i < nelem(default_files); i++) {
            char *p = resolve_inner(homedir, default_files[i]);
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
