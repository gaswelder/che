#import fileutil
#import fs
#import mime
#import os/misc
#import os/net
#import strings
#import time
#import http

#import lkbuffer.c
#import lkconfig.c
#import lkcontext.c
#import lkhostconfig.c
#import lkhttpcgiparser.c
#import lknet.c
#import lkreflist.c
#import lkstring.c
#import lkstringtable.c
#import request.c
#import srvfiles.c


#define LK_BUFSIZE_SMALL 512
#define TIME_STRING_SIZE 25
#define MAX_CLIENTS 1000

enum {FD_SOCK, FD_FILE};



pub typedef {
    lkconfig.LKConfig *cfg;
    lkcontext.LKContext *ctxhead;
    
    // Sparse list of client connections.
    net.net_t *clients[MAX_CLIENTS];
} LKHttpServer;

pub bool lk_httpserver_serve(lkconfig.LKConfig *cfg) {
    LKHttpServer httpserver = {
        .cfg = cfg,
        .ctxhead = NULL
    };

    /*
     * Get the hostname and set common env for future CGI children.
     */
    char hostname[1024] = {0};
    if (!misc.get_hostname(hostname, sizeof(hostname))) {
        fprintf(stderr, "failed to get hostname: %s\n", strerror(errno));
        exit(1);
    }
    printf("hostname = %s\n", hostname);
    misc.clearenv();
    misc.setenv("SERVER_NAME", hostname, 1);
    misc.setenv("SERVER_SOFTWARE", "littlekitten/0.1", 1);
    misc.setenv("SERVER_PROTOCOL", "HTTP/1.0", 1);
    misc.setenv("SERVER_PORT", cfg->port, 1);

    /*
     * Start listening and create a pool for incoming connections.
     */
    char *addr = strings.newstr("%s:%s", cfg->serverhost, cfg->port);
    net.net_t *listen_conn = net.net_listen("tcp", addr);
    if (!listen_conn) {
        free(addr);
        return false;
    }
    printf("Serving at http://%s\n", net.net_addr(listen_conn));
    free(addr);
    net.pool_t *p = net.new_pool();
    if (!p) {
        fprintf(stderr, "failed to create a connections pool\n");
        exit(1);
    }
    net.pool_add(p, listen_conn);
    

    while (true) {
        net.event_t *ev = net.get_event(p);
        printf("got event, conn = %d, read = %d\n", ev->conn->fd, ev->read);
        if (ev->conn == listen_conn && ev->read) {
            printf("accepting on %d\n", ev->conn->fd);
            net.net_t *client = net.net_accept(listen_conn);
            if (!client) {
                fprintf(stderr, "accept failed: %s, %s\n", net.net_error(), strerror(errno));
                continue;
            }
            // hack
            int clientfd = client->fd;
            lkcontext.LKContext *ctx = lkcontext.create_initial_context(clientfd);
            ctx->conn = client;
            lkcontext.add_new_client_context(&httpserver.ctxhead, ctx);
            net.pool_add(p, client);
            continue;
        }

        // hack
        int selectfd = ev->conn->fd;
        lkcontext.LKContext *ctx = lkcontext.match_select_ctx(httpserver.ctxhead, selectfd);
        if (ctx == NULL) {
            printf("selectfd %d not in ctx list\n", selectfd);
            terminate_sock(selectfd, FD_READ, &httpserver);
            net.pool_remove(p, ev->conn);
            continue;
        }
        if (ev->read) {
            process_readable_client(&httpserver, ctx);
        }
        if (ev->write) {
            process_writable_client(&httpserver, ctx);
        }
    }

    lkconfig.lk_config_free(httpserver.cfg);
    lkcontext.LKContext *ctx = httpserver.ctxhead;
    while (ctx != NULL) {
        lkcontext.LKContext *ptmp = ctx;
        ctx = ctx->next;
        lkcontext.lk_context_free(ptmp);
    }
    memset(&httpserver, 0, sizeof(LKHttpServer));
    return true;
}

void process_readable_client(LKHttpServer *httpserver, lkcontext.LKContext *ctx) {
    printf("process_readable_client\n");
    if (ctx->type == lkcontext.CTX_READ_REQ) {
        printf("read_request\n");
        read_request(httpserver, ctx, ctx->conn);
    } else if (ctx->type == lkcontext.CTX_READ_CGI_OUTPUT) {
        read_cgi_output(httpserver, ctx);
    } else if (ctx->type == lkcontext.CTX_PROXY_PIPE_RESP) {
        pipe_proxy_response(httpserver, ctx);
    } else {
        printf("process_readable_client: unknown ctx type %d\n", ctx->type);
    }
    // net.pool_remove(p, conn);
}

void process_writable_client(LKHttpServer *server, lkcontext.LKContext *ctx) {
    if (ctx->type == lkcontext.CTX_WRITE_DATA) {
        assert(ctx->resp != NULL);
        assert(ctx->resp->head != NULL);
        write_response(server, ctx);
    } else if (ctx->type == lkcontext.CTX_WRITE_CGI_INPUT) {
        write_cgi_input(server, ctx);
    } else if (ctx->type == lkcontext.CTX_PROXY_WRITE_REQ) {
        assert(ctx->req != NULL);
        assert(ctx->req->head != NULL);
        write_proxy_request(server, ctx);
    } else {
        printf("write_client: unknown ctx type %d\n", ctx->type);
    }
}


// Sets the cgi environment variables that vary for each http request.
void set_cgi_env2(lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    request.LKHttpRequest *wreq = ctx->req;
    http.request_t *req = &wreq->req;

    misc.setenv("DOCUMENT_ROOT", hc->homedir_abspath->s, 1);

    http.header_t *h = NULL;
    
    h = http.get_header(req, "User-Agent");
    if (h) {
        misc.setenv("HTTP_USER_AGENT", h->value, 1);
    } else {
        misc.setenv("HTTP_USER_AGENT", "", 1);
    }

    h = http.get_header(req, "Host");
    if (h) {
        misc.setenv("HTTP_HOST", h->value, 1);
    } else {
        misc.setenv("HTTP_HOST", "", 1);
    }

    h = http.get_header(req, "Content-Type");
    if (h) {
        misc.setenv("CONTENT_TYPE", h->value, 1);
    } else {
        misc.setenv("CONTENT_TYPE", "", 1);
    }

    
    

    lkstring.LKString *lkscript_filename = lkstring.lk_string_new(hc->homedir_abspath->s);
    lkstring.lk_string_append(lkscript_filename, req->path);
    misc.setenv("SCRIPT_FILENAME", lkscript_filename->s, 1);
    lkstring.lk_string_free(lkscript_filename);

    misc.setenv("REQUEST_METHOD", req->method, 1);
    misc.setenv("SCRIPT_NAME", req->path, 1);
    misc.setenv("REQUEST_URI", req->uri, 1);
    misc.setenv("QUERY_STRING", req->query, 1);

    

    char content_length[10];
    snprintf(content_length, sizeof(content_length), "%ld", wreq->body->bytes_len);
    content_length[sizeof(content_length)-1] = '\0';
    misc.setenv("CONTENT_LENGTH", content_length, 1);

    misc.setenv("REMOTE_ADDR", ctx->client_ipaddr->s, 1);
    char portstr[10];
    snprintf(portstr, sizeof(portstr), "%d", ctx->client_port);
    misc.setenv("REMOTE_PORT", portstr, 1);
}

void read_request(LKHttpServer *server, lkcontext.LKContext *ctx, net.net_t *client_conn) {
    // Put incoming data into the client's buffer.
    int r = net.readconn(client_conn,
        ctx->input_buffer + ctx->input_buffer_len,
        sizeof(ctx->input_buffer) - ctx->input_buffer_len);
    if (r < 0) {
        fprintf(stderr, "readconn failed: %s\n", strerror(errno));
        return;
    }
    ctx->input_buffer_len += r;
    char tmp[4096] = {0};
    memcpy(tmp, ctx->input_buffer, ctx->input_buffer_len);
    fprintf(stderr, "got %d bytes of data: %s\n", r, tmp);

    bool ok = true;
    while (ok) {
        // Try processing the new data.
        // If it doesn't return true, the new data wasn't enough to change
        // the state, so return until more data comes in.
        printf("ctx state = %d\n", ctx->type);
        switch (ctx->type) {
        case lkcontext.CTX_READ_REQ:
            ok = try_read_header(ctx);
            break;
        case lkcontext.CTX_RESOLVE_REQ:
            ok = process_request(server, ctx);
            break;
        case lkcontext.CTX_WRITE_DATA:
            ok = write_output(ctx);
            break;
        default:
            panic("unhandled ctx state: %d", ctx->type);
        }
    }
}

bool write_output(lkcontext.LKContext *ctx) {
    printf("writing output\n");
    printf("output size = %zu\n", ctx->output_buffer_len);
    int n = net.net_write(ctx->conn, ctx->output_buffer, ctx->output_buffer_len);
    printf("written %d bytes\n", n);
    net.net_close(ctx->conn);
    return false;
}

bool try_read_header(lkcontext.LKContext *ctx) {
    fprintf(stderr, "reading head\n");

    char *split = strstr(ctx->input_buffer, "\r\n\r\n");
    if (!split || (size_t)(split - ctx->input_buffer) > ctx->input_buffer_len) {
        fprintf(stderr, "no complete line yet\n");
        return false;
    }
    split += 4;

    char head[4096] = {0};
    size_t headlen = split - ctx->input_buffer;
    memcpy(head, ctx->input_buffer, headlen);
    fprintf(stderr, "got head: -----[%s]----\n", head);
    

    /*
     * Parse the request
     */
    if (!http.parse_request(&ctx->req->req, head)) {
        panic("failed to parse the request");
    }

    // Shift the buffer data
    ctx->input_buffer_len -= headlen;
    memmove(ctx->input_buffer, split, ctx->input_buffer_len);

    // Switch to service the request.
    ctx->type = lkcontext.CTX_RESOLVE_REQ;
    return true;
}

// Send cgi_inputbuf input bytes to cgi program stdin set in selectfd.
void write_cgi_input(LKHttpServer *server, lkcontext.LKContext *ctx) {
    assert(ctx->cgi_inputbuf != NULL);
    int z = lknet.lk_write_all_file(ctx->selectfd, ctx->cgi_inputbuf);
    if (z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lk_print_err("write_cgi_input lknet.lk_write_all_file()");
        z = terminate_fd(ctx->cgifd, FD_WRITE, server);
        if (z == 0) {
            ctx->cgifd = 0;
        }
        lkcontext.remove_selectfd_context(&server->ctxhead, ctx->selectfd);
        return;
    }
    if (z == lknet.Z_EOF) {
        // Completed writing input bytes.
        // FD_CLR_WRITE(ctx->selectfd, server);
        // shutdown(ctx->selectfd, SHUT_WR);
        lkcontext.remove_selectfd_context(&server->ctxhead, ctx->selectfd);
    }
}

// Read cgi output to cgi_outputbuf.
void read_cgi_output(LKHttpServer *server, lkcontext.LKContext *ctx) {
    int z = lknet.lk_read_all_file(ctx->selectfd, ctx->cgi_outputbuf);
    if (z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lk_print_err("lk_read_all_file()");
        z = terminate_fd(ctx->cgifd, FD_READ, server);
        if (z == 0) {
            ctx->cgifd = 0;
        }
        process_error_response(server, ctx, 500, "Error processing CGI output.");
        return;
    }

    // EOF - finished reading cgi output.
    assert(z == 0);

    // Remove cgi output from read list.
    z = terminate_fd(ctx->cgifd, FD_READ, server);
    if (z == 0) {
        ctx->cgifd = 0;
    }

    lkhttpcgiparser.parse_cgi_output(ctx->cgi_outputbuf, ctx->resp);
    process_response(server, ctx);
}

bool process_request(LKHttpServer *server, lkcontext.LKContext *ctx) {
    printf("processing request\n");
    http.request_t *req = &ctx->req->req;
    char *hostname = NULL;
    http.header_t *host = http.get_header(req, "Host");
    if (host) {
        hostname = host->value;
    }
    printf("hostname = %s\n", hostname);

    lkhostconfig.LKHostConfig *hc = lkconfig.lk_config_find_hostconfig(server->cfg, hostname);
    if (hc == NULL) {
        printf("no host config\n");
        process_error_response(server, ctx, 404, "LittleKitten webserver: hostconfig not found.");
        return true;
    }
    printf("got host config\n");

    // Forward request to proxyhost if proxyhost specified.
    if (hc->proxyhost->s_len > 0) {
        panic("todo");
        serve_proxy(server, ctx, hc->proxyhost->s);
        return true;
    }

    if (hc->homedir->s_len == 0) {
        panic("todo");
        process_error_response(server, ctx, 404, "LittleKitten webserver: hostconfig homedir not specified.");
        return true;
    }

    // Replace path with any matching alias.
    char *match = lkstringtable.lk_stringtable_get(hc->aliases, req->path);
    if (match != NULL) {
        panic("todo");
        if (strlen(match) + 1 > sizeof(req->path)) {
            abort();
        }
        strcpy(req->path, match);
    }

    // Run cgi script if uri falls under cgidir
    if (hc->cgidir->s_len > 0 && strings.starts_with(req->path, hc->cgidir->s)) {
        panic("todo");
        serve_cgi(server, ctx, hc);
        return true;
    }

    return resolve_request(ctx, hc);
    // process_response(server, ctx);
    // return true;
}

const char *NOT_FOUND_MESSAGE = "File not found on the server.";

bool resolve_request(lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    printf("resolve_request\n");
    http.request_t *req = &ctx->req->req;
    ctx->output_buffer_len = 0;

    if (!strcmp(req->method, "GET")) {
        printf("processing GET\n");
        char *filepath = srvfiles.resolve(hc->homedir->s, req->path);
        if (!filepath) {
            char *p = ctx->output_buffer;
            p += sprintf(p, "%s 404 Not Found\n", req->version);
            p += sprintf(p, "Content-Length: %ld\n", strlen(NOT_FOUND_MESSAGE));
            p += sprintf(p, "Content-Type: text/plain\n");
            p += sprintf(p, "\n");
            p += sprintf(p, "%s", NOT_FOUND_MESSAGE);
            ctx->output_buffer_len = p - ctx->output_buffer;
            printf("written %zu bytes to the output buffer\n", ctx->output_buffer_len);
            ctx->type = lkcontext.CTX_WRITE_DATA;
            return true;
        }

        // Read the file
        size_t filesize = 0;
        char *data = fileutil.readfile(filepath, &filesize);
        if (!data) {
            panic("failed to read file '%s'", filepath);
        }
        const char *content_type = mime.lookup(fileext(filepath));
        if (content_type == NULL) {
            content_type = "text/plain";
        }

        // Format the response.
        char *p = ctx->output_buffer;
        p += sprintf(p, "%s 200 OK\n", req->version);
        p += sprintf(p, "Content-Length: %ld\n", filesize);
        p += sprintf(p, "Content-Type: %s\n", content_type);
        p += sprintf(p, "\n");
        memcpy(p, data, filesize);
        ctx->output_buffer_len = filesize + (p - ctx->output_buffer);
        ctx->type = lkcontext.CTX_WRITE_DATA;
        free(filepath);
        return true;
    }

    panic("unsupported method: '%s'", req->method);
    return false;
    

    
    // request.LKHttpResponse *resp = ctx->resp;
    // const char *method = req->method;
    // const char *path = req->path;

    
    // if (POSTTEST) {
    //     if (!strcmp(method, "POST")) {
    //         char *html_start =
    //         "<!DOCTYPE html>\n"
    //         "<html>\n"
    //         "<head><title>Little Kitten Sample Response</title></head>\n"
    //         "<body>\n";
    //         char *html_end =
    //         "</body></html>\n";

    //         lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/html");
    //         lkbuffer.lk_buffer_append(resp->body, html_start, strlen(html_start));
    //         lkbuffer.lk_buffer_append_sz(resp->body, "<pre>\n");
    //         lkbuffer.lk_buffer_append(resp->body, ctx->req->body->bytes, ctx->req->body->bytes_len);
    //         lkbuffer.lk_buffer_append_sz(resp->body, "\n</pre>\n");
    //         lkbuffer.lk_buffer_append(resp->body, html_end, strlen(html_end));
    //         return;
    //     }
    // }

    // resp->status = 501;
    // lkstring.lk_string_assign_sprintf(resp->statustext, "Unsupported method ('%s')", method);

// char *html_error_start = 
//        "<!DOCTYPE html>\n"
//        "<html>\n"
//        "<head><title>Error response</title></head>\n"
//        "<body><h1>Error response</h1>\n";
//     char *html_error_end =
//        "</body></html>\n";
    // lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/html");
    // lkbuffer.lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
    // lkbuffer.lk_buffer_append_sprintf(resp->body, "<p>Error code %d.</p>\n", resp->status);
    // lkbuffer.lk_buffer_append_sprintf(resp->body, "<p>Message: Unsupported method ('%s').</p>\n", resp->statustext->s);
    // lkbuffer.lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
}

void serve_cgi(LKHttpServer *server, lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    request.LKHttpRequest *req = ctx->req;
    request.LKHttpResponse *resp = ctx->resp;
    char *path = ctx->req->req.path;

    lkstring.LKString *cgifile = lkstring.lk_string_new(hc->homedir_abspath->s);
    lkstring.lk_string_append(cgifile, path);

    // Expand "/../", etc. into real_path.
    char real_path[PATH_MAX];
    bool pathok = fs.realpath(cgifile->s, real_path, sizeof(real_path));
    lkstring.lk_string_free(cgifile);

    // real_path should start with cgidir_abspath
    // real_path file should exist
    if (!pathok
        || !strings.starts_with(real_path, hc->cgidir_abspath->s)
        || !fileutil.file_exists(real_path)
    ) {
        resp->status = 404;
        lkstring.lk_string_assign_sprintf(resp->statustext, "File not found '%s'", path);
        lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lkbuffer.lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", path);

        process_response(server, ctx);
        return;
    }

    set_cgi_env2(ctx, hc);

    // cgi stdout and stderr are streamed to fd_out.
    //$$todo pass any request body to fd_in.
    int fd_in, fd_out;
    int z = lk_popen3(real_path, &fd_in, &fd_out, NULL);
    if (z == -1) {
        resp->status = 500;
        lkstring.lk_string_assign_sprintf(resp->statustext, "Server error '%s'", strerror(errno));
        lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lkbuffer.lk_buffer_append_sprintf(resp->body, "Server error '%s'\n", strerror(errno));
        process_response(server, ctx);
        return;
    }

    OS.close(fd_in);

    // Read cgi output in select()
    ctx->selectfd = fd_out;
    ctx->cgifd = fd_out;
    ctx->type = lkcontext.CTX_READ_CGI_OUTPUT;
    ctx->cgi_outputbuf = lkbuffer.lk_buffer_new(0);

    // If req is POST with body, pass it to cgi process stdin.
    if (req->body->bytes_len > 0) {
        lkcontext.LKContext *ctx_in = lkcontext.lk_context_new();
        lkcontext.add_context(&server->ctxhead, ctx_in);

        ctx_in->selectfd = fd_in;
        ctx_in->cgifd = fd_in;
        ctx_in->clientfd = ctx->clientfd;
        ctx_in->type = lkcontext.CTX_WRITE_CGI_INPUT;

        ctx_in->cgi_inputbuf = lkbuffer.lk_buffer_new(0);
        lkbuffer.lk_buffer_append(ctx_in->cgi_inputbuf, req->body->bytes, req->body->bytes_len);

        // FD_SET_WRITE(ctx_in->selectfd, server);
    }
}

void process_response(LKHttpServer *server, lkcontext.LKContext *ctx) {
    (void) server;
    http.request_t *req = &ctx->req->req;
    request.LKHttpResponse *resp = ctx->resp;

    lk_httpresponse_finalize(resp);

    // Clear response body on HEAD request.
    if (!strcmp(req->method, "HEAD")) {
        lkbuffer.lk_buffer_clear(resp->body);
    }

    char time_str[TIME_STRING_SIZE];
    get_localtime_string(time_str, sizeof(time_str));
    printf("%s [%s] \"%s %s %s\" %d\n", 
        ctx->client_ipaddr->s, time_str,
        req->method, req->uri, resp->version->s,
        resp->status);
    if (resp->status >= 500 && resp->status < 600 && resp->statustext->s_len > 0) {
        printf("%s [%s] %d - %s\n", 
            ctx->client_ipaddr->s, time_str,
            resp->status, resp->statustext->s);
    }

    ctx->selectfd = ctx->clientfd;
    ctx->type = lkcontext.CTX_WRITE_DATA;
    // FD_SET_WRITE(ctx->selectfd, server);
    lkreflist.lk_reflist_clear(ctx->buflist);
    lkreflist.lk_reflist_append(ctx->buflist, resp->head);
    lkreflist.lk_reflist_append(ctx->buflist, resp->body);
    return;
}

// Finalize the http response by setting head buffer.
// Writes the status line, headers and CRLF blank string to head buffer.
void lk_httpresponse_finalize(request.LKHttpResponse *resp) {
    lkbuffer.lk_buffer_clear(resp->head);

    // Default to 200 OK if no status set.
    if (resp->status == 0) {
        resp->status = 200;
        lkstring.lk_string_assign(resp->statustext, "OK");
    }
    // Default to HTTP version.
    if (lkstring.lk_string_sz_equal(resp->version, "")) {
        lkstring.lk_string_assign(resp->version, "HTTP/1.0");
    }
    lkbuffer.lk_buffer_append_sprintf(resp->head, "%s %d %s\n", resp->version->s, resp->status, resp->statustext->s);
    lkbuffer.lk_buffer_append_sprintf(resp->head, "Content-Length: %ld\n", resp->body->bytes_len);
    for (size_t i=0; i < resp->headers->items_len; i++) {
        lkbuffer.lk_buffer_append_sprintf(resp->head, "%s: %s\n", resp->headers->items[i].k->s, resp->headers->items[i].v->s);
    }
    lkbuffer.lk_buffer_append(resp->head, "\r\n", 2);
}

void process_error_response(LKHttpServer *server, lkcontext.LKContext *ctx, int status, char *msg) {
    request.LKHttpResponse *resp = ctx->resp;
    resp->status = status;
    lkstring.lk_string_assign(resp->statustext, msg);
    lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
    lkbuffer.lk_buffer_append_sz(resp->body, msg);

    process_response(server, ctx);
}




// Return ptr to start of file extension within filepath.
// Ex. "path/to/index.html" returns "html"
const char *fileext(const char *filepath) {
    int filepath_len = strlen(filepath);
    // filepath of "" returns ext of "".
    if (filepath_len == 0) {
        return filepath;
    }

    const char *p = filepath + strlen(filepath) - 1;
    while (p >= filepath) {
        if (*p == '.') {
            return p+1;
        }
        p--;
    }
    return filepath;
}

void write_response(LKHttpServer *server, lkcontext.LKContext *ctx) {
    int z = lk_buflist_write_all(ctx->selectfd, FD_SOCK, ctx->buflist);
    if (z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lk_print_err("write_response lk_buflist_write_all()");
        terminate_client_session(server, ctx);
        return;
    }
    if (z == lknet.Z_EOF) {
        // Completed sending http response.
        terminate_client_session(server, ctx);
    }
}

void serve_proxy(LKHttpServer *server, lkcontext.LKContext *ctx, char *targethost) {
    net.net_t *proxyconn = net.net_open("tcp", targethost);
    if (!proxyconn) {
        lk_print_err("lk_open_connect_socket()");
        process_error_response(server, ctx, 500, "Error opening proxy socket.");
        return;
    }

    // hack
    int proxyfd = proxyconn->fd;

    lk_httprequest_finalize(ctx->req);
    ctx->proxyfd = proxyfd;
    ctx->selectfd = proxyfd;
    ctx->type = lkcontext.CTX_PROXY_WRITE_REQ;
    // FD_SET_WRITE(proxyfd, server);
    lkreflist.lk_reflist_clear(ctx->buflist);
    lkreflist.lk_reflist_append(ctx->buflist, ctx->req->head);
    lkreflist.lk_reflist_append(ctx->buflist, ctx->req->body);
}

void lk_httprequest_finalize(request.LKHttpRequest *wreq) {
    http.request_t *req = &wreq->req;
    lkbuffer.lk_buffer_clear(wreq->head);

    // Default to HTTP version.
    if (!strcmp(req->version, "")) {
        strcpy(req->version, "HTTP/1.0");
    }
    lkbuffer.lk_buffer_append_sprintf(wreq->head, "%s %s %s\n", req->method, req->uri, req->version);
    if (wreq->body->bytes_len > 0) {
        lkbuffer.lk_buffer_append_sprintf(wreq->head, "Content-Length: %ld\n", wreq->body->bytes_len);
    }
    for (size_t i = 0; i < req->nheaders; i++) {
        http.header_t *h = &req->headers[i];
        lkbuffer.lk_buffer_append_sprintf(wreq->head, "%s: %s\n", h->name, h->value);
    }
    lkbuffer.lk_buffer_append(wreq->head, "\r\n", 2);
}

void write_proxy_request(LKHttpServer *server, lkcontext.LKContext *ctx) {
    int z = lk_buflist_write_all(ctx->selectfd, FD_SOCK, ctx->buflist);
    if (z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lk_print_err("write_proxy_request lk_buflist_write_all");
        z = terminate_sock(ctx->proxyfd, FD_WRITE, server);
        if (z == 0) {
            ctx->proxyfd = 0;
        }
        process_error_response(server, ctx, 500, "Error forwarding request to proxy.");
        return;
    }
    if (z == lknet.Z_EOF) {
        // Completed sending http request.
        // FD_CLR_WRITE(ctx->selectfd, server);
        // shutdown(ctx->selectfd, SHUT_WR);

        // Pipe proxy response from ctx->proxyfd to ctx->clientfd
        ctx->type = lkcontext.CTX_PROXY_PIPE_RESP;
        ctx->proxy_respbuf = lkbuffer.lk_buffer_new(0);
        // FD_SET_READ(ctx->selectfd, server);
    }
}

// Similar to lknet.lk_write_all(), but sending buflist buf's sequentially.
int lk_buflist_write_all(int fd, int fd_type, lkreflist.LKRefList *buflist) {
    if (buflist->items_cur >= buflist->items_len) {
        return lknet.Z_EOF;
    }

    lkbuffer.LKBuffer *buf = buflist->items[buflist->items_cur];
    int z = lknet.lk_write_all(fd, fd_type, buf);
    if (z == lknet.Z_EOF) {
        buflist->items_cur++;
        z = lknet.Z_OPEN;
    }
    return z;
}

void pipe_proxy_response(LKHttpServer *server, lkcontext.LKContext *ctx) {
    int z = lk_pipe_all(ctx->proxyfd, ctx->clientfd, FD_SOCK, ctx->proxy_respbuf);
    if (z == lknet.Z_OPEN || z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lk_print_err("pipe_proxy_response lk_pipe_all()");
        z = terminate_sock(ctx->proxyfd, FD_READ, server);
        if (z == 0) {
            ctx->proxyfd = 0;
        }
        process_error_response(server, ctx, 500, "Error reading/writing proxy response.");
        return;
    }

    // EOF - finished reading/writing proxy response.
    assert(z == lknet.Z_EOF);

    // Remove proxy from read list.
    z = terminate_sock(ctx->proxyfd, FD_READ, server);
    if (z == 0) {
        ctx->proxyfd = 0;
    }

    http.request_t *req = &ctx->req->req;
    char time_str[TIME_STRING_SIZE];
    get_localtime_string(time_str, sizeof(time_str));
    printf("%s [%s] \"%s %s\" --> proxyhost\n",
        ctx->client_ipaddr->s, time_str, req->method, req->uri);

    // Completed sending proxy response.
    terminate_client_session(server, ctx);
}

enum {
    FD_READ,
    FD_WRITE,
    FD_READWRITE
}; // fd_action

// Clear fd from select()'s, shutdown, and OS.close.
int terminate_fd(int fd, int fd_action, LKHttpServer *server) {
    (void) server;
    int z;
    if (fd_action == FD_READ || fd_action == FD_READWRITE) {
        // FD_CLR_READ(fd, server);
    }
    if (fd_action == FD_WRITE || fd_action == FD_READWRITE) {
        // FD_CLR_WRITE(fd, server);
    }
    z = OS.close(fd);
    if (z == -1) {
        lk_print_err("OS.close()");
    }
    return z;
}

int terminate_sock(int fd, int fd_action, LKHttpServer *server) {
    (void) server;
    if (fd_action == FD_READ || fd_action == FD_READWRITE) {
        // FD_CLR_READ(fd, server);
    }
    if (fd_action == FD_WRITE || fd_action == FD_READWRITE) {
        // FD_CLR_WRITE(fd, server);
    }
    int z = OS.close(fd);
    if (z == -1) {
        lk_print_err("OS.close()");
    }
    return z;
}

// Disconnect from client.
void terminate_client_session(LKHttpServer *server, lkcontext.LKContext *ctx) {
    if (ctx->clientfd) {
        terminate_sock(ctx->clientfd, FD_READWRITE, server);
    }
    if (ctx->cgifd) {
        terminate_fd(ctx->cgifd, FD_READWRITE, server);
    }
    if (ctx->proxyfd) {
        terminate_sock(ctx->proxyfd, FD_READWRITE, server);
    }
    // Remove from linked list and free ctx.
    lkcontext.remove_client_context(&server->ctxhead, ctx->clientfd);
}



// localtime in server format: 11/Mar/2023 14:05:46
void get_localtime_string(char *buf, size_t size) {
    if (!time.time_format(buf, size, "%d/%b/%Y %H:%M:%S")) {
        snprintf(buf, size, "failed to get time: %s", strerror(errno));
    }
}

// Pipe all available nonblocking readfd bytes into writefd.
// Uses buf as buffer for queued up bytes waiting to be written.
// Returns one of the following:
//    0 (lknet.Z_EOF) for read/write complete.
//    1 (lknet.Z_OPEN) for writefd socket OS.open
//   -1 (lknet.Z_ERR) for read/write error.
//   -2 (lknet.Z_BLOCK) for blocked readfd/writefd socket
int lk_pipe_all(int readfd, int writefd, int fd_type, lkbuffer.LKBuffer *buf) {
    int readz, writez;

    readz = lknet.lk_read_all(readfd, fd_type, buf);
    if (readz == lknet.Z_ERR) {
        return readz;
    }
    assert(readz == lknet.Z_EOF || readz == lknet.Z_BLOCK);

    writez = lknet.lk_write_all(writefd, fd_type, buf);
    if (writez == lknet.Z_ERR) {
        return writez;
    }

    // Pipe complete if no more data to read and all bytes sent.
    if (readz == lknet.Z_EOF && writez == lknet.Z_EOF) {
        return lknet.Z_EOF;
    }

    if (readz == lknet.Z_BLOCK) {
        return lknet.Z_BLOCK;
    }
    if (writez == lknet.Z_EOF) {
        writez = lknet.Z_OPEN;
    }
    return writez;
}

// Like popen() but returning input, output, error fds for cmd.
int lk_popen3(char *cmd, int *fd_in, int *fd_out, int *fd_err) {
    int z;
    int in[2] = {0, 0};
    int out[2] = {0, 0};
    int err[2] = {0, 0};

    z = OS.pipe(in);
    if (z == -1) {
        return z;
    }
    z = OS.pipe(out);
    if (z == -1) {
        close_pipes(in, out, err);
        return z;
    }
    z = OS.pipe(err);
    if (z == -1) {
        close_pipes(in, out, err);
        return z;
    }

    int pid = OS.fork();
    if (pid == 0) {
        // child proc
        z = OS.dup2(in[0], OS.STDIN_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }
        z = OS.dup2(out[1], OS.STDOUT_FILENO);
        if (z == -1) {
            close_pipes(in, out, err);
            return z;
        }
        // If fd_err parameter provided, use separate fd for stderr.
        // If fd_err is NULL, combine stdout and stderr into fd_out.
        if (fd_err != NULL) {
            z = OS.dup2(err[1], OS.STDERR_FILENO);
            if (z == -1) {
                close_pipes(in, out, err);
                return z;
            }
        } else {
            z = OS.dup2(out[1], OS.STDERR_FILENO);
            if (z == -1) {
                close_pipes(in, out, err);
                return z;
            }
        }

        close_pipes(in, out, err);
        z = OS.execl("/bin/sh", "sh",  "-c", cmd, NULL);
        return z;
    }

    // parent proc
    if (fd_in != NULL) {
        *fd_in = in[1]; // return the other end of the OS.dup2() OS.pipe
    }
    OS.close(in[0]);

    if (fd_out != NULL) {
        *fd_out = out[0];
    }
    OS.close(out[1]);

    if (fd_err != NULL) {
        *fd_err = err[0];
    }
    OS.close(err[1]);

    return 0;
}

void close_pipes(int pair1[2], int pair2[2], int pair3[2]) {
    int z;
    int tmp_errno = errno;
    if (pair1[0] != 0) {
        z = OS.close(pair1[0]);
        if (z == 0) {
            pair1[0] = 0;
        }
    }
    if (pair1[1] != 0) {
        z = OS.close(pair1[1]);
        if (z == 0) {
            pair1[1] = 0;
        }
    }
    if (pair2[0] != 0) {
        z = OS.close(pair2[0]);
        if (z == 0) {
            pair2[0] = 0;
        }
    }
    if (pair2[1] != 0) {
        z = OS.close(pair2[1]);
        if (z == 0) {
            pair2[1] = 0;
        }
    }
    if (pair3[0] != 0) {
        z = OS.close(pair3[0]);
        if (z == 0) {
            pair3[0] = 0;
        }
    }
    if (pair3[1] != 0) {
        z = OS.close(pair3[1]);
        if (z == 0) {
            pair3[1] = 0;
        }
    }
    errno = tmp_errno;
    return;
}

void lk_print_err(char *s) {
    fprintf(stderr, "%s: %s\n", s, strerror(errno));
}