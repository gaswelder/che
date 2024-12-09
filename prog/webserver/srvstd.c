#import http
#import os/net
#import server.c

pub void write_404(http.request_t *req, server.ctx_t *ctx) {
    const char *msg = "The file was not found on the server.";
	char buf[1000] = {};
	sprintf(buf,
        "%s 404 Not Found\n"
        "Content-Length: %ld\n"
        "Content-Type: text/plain\n"
        "\n"
        "\n"
        "%s",
        req->version,
        strlen(msg),
        msg
    );
    int r = net.net_write(ctx->conn, buf, strlen(buf));
	if (r < 0) {
		panic("net_write failed");
	}
}

pub void write_405(http.request_t *req, server.ctx_t *ctx) {
    const char *msg = "This method is not allowed for this path.";
	char buf[1000] = {};
    sprintf(buf,
        "%s 405 Method Not Allowed\n"
        "Content-Length: %ld\n"
        "Content-Type: text/plain\n"
        "\n"
        "\n"
        "%s",
        req->version,
        strlen(msg),
        msg
    );
    int r = net.net_write(ctx->conn, buf, strlen(buf));
	if (r < 0) {
		panic("net_write failed");
	}
}

pub void write_501(http.request_t *req, server.ctx_t *ctx) {
    const char *msg = "method not implemented\n";
	char buf[1000] = {};
    sprintf(buf,
        "%s 501 Not Implemented\n"
        "Content-Length: %ld\n"
        "Content-Type: text/plain\n"
        "\n"
        "\n"
        "%s",
        req->version,
        strlen(msg),
        msg
    );
    int r = net.net_write(ctx->conn, buf, strlen(buf));
	if (r < 0) {
		panic("net_write failed");
	}
}
