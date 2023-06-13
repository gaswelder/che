#import fileutil
#import mime
#import os/misc
#import os/net
#import strings
#import time

#import lkalloc.c
#import lkbuffer.c
#import lkconfig.c
#import lkcontext.c
#import lkhostconfig.c
#import lklib.c
#import lknet.c
#import lkreflist.c
#import request.c


enum {FD_SOCK, FD_FILE};

/*** LKContext ***/
enum {
    CTX_READ_REQ,
    CTX_READ_CGI_OUTPUT,
    CTX_WRITE_CGI_INPUT,
    CTX_WRITE_RESP,
    CTX_PROXY_WRITE_REQ,
    CTX_PROXY_PIPE_RESP
}; // LKContextType;

pub typedef {
    lkconfig.LKConfig *cfg;
    lkcontext.LKContext *ctxhead;
    // The listening connection.
    net.net_t *listen_conn;
} LKHttpServer;

pub int lk_httpserver_serve(lkconfig.LKConfig *cfg) {
    LKHttpServer httpserver = {
        .cfg = cfg,
        .ctxhead = NULL
    };

    char *addr = strings.newstr("%s:%s", cfg->serverhost->s, cfg->port->s);
    httpserver->listen_conn = net.net_listen("tcp", addr);
    if (!httpserver->listen_conn) {
        free(addr);
        return -1;
    }
    printf("Serving HTTP at %s\n", addr);
    free(addr);

    clearenv();
    set_cgi_env1(server);

    while (true) {
        // put all connections into set
        net.net_t *set[100] = {};
        int n = 0;
        for (int i = 0; i < 100; i++) {
            if (!server->connections[i]) {
                continue;
            }
            set[n++] = connections[i];
            if (n == 100) {
                abort("too many connections");
            }
        }

        if (!update_socket_status(set)) {
            if (errno == EINTR) {
                continue;
            }
            lklib.lk_print_err("select()");
            return -1;
        }

        if (server->connections[0]->is_readable) {
            net_t *client = net.net_accept(server->listen_conn);
            if (!client) {
                lklib.lk_print_err("accept()");
                continue;
            }
            add_client(httpserver, client);
        }

        for (int i = 1; i < 100; i++) {
            net.net_t *conn = set[i];
            if (!conn) {
                continue;
            }
            if (conn->is_readable) {
                read_client();
            }
            if (conn->is_writable && havedata) {
                write();
            }
        }
    }

    lkconfig.lk_config_free(server->cfg);

    // Free ctx linked list
    lkcontext.LKContext *ctx = server->ctxhead;
    while (ctx != NULL) {
        lkcontext.LKContext *ptmp = ctx;
        ctx = ctx->next;
        lk_context_free(ptmp);
    }
    memset(server, 0, sizeof(LKHttpServer));
    return 0;
}

// Sets the cgi environment variables that stay the same across http requests.
void set_cgi_env1(LKHttpServer *server) {
    int z;
    lkconfig.LKConfig *cfg = server->cfg;

    char hostname[LK_BUFSIZE_SMALL];
    z = gethostname(hostname, sizeof(hostname)-1);
    if (z == -1) {
        lklib.lk_print_err("gethostname()");
        hostname[0] = '\0';
    }
    hostname[sizeof(hostname)-1] = '\0';
    
    misc.setenv("SERVER_NAME", hostname, 1);
    misc.setenv("SERVER_SOFTWARE", "littlekitten/0.1", 1);
    misc.setenv("SERVER_PROTOCOL", "HTTP/1.0", 1);
    misc.setenv("SERVER_PORT", cfg->port->s, 1);

}

// Sets the cgi environment variables that vary for each http request.
void set_cgi_env2(LKHttpServer *server, lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    request.LKHttpRequest *req = ctx->req;

    misc.setenv("DOCUMENT_ROOT", hc->homedir_abspath->s, 1);

    char *http_user_agent = lk_stringtable_get(req->headers, "User-Agent");
    if (!http_user_agent) http_user_agent = "";
    misc.setenv("HTTP_USER_AGENT", http_user_agent, 1);

    char *http_host = lk_stringtable_get(req->headers, "Host");
    if (!http_host) http_host = "";
    misc.setenv("HTTP_HOST", http_host, 1);

    lkstring.LKString *lkscript_filename = lk_string_new(hc->homedir_abspath->s);
    lk_string_append(lkscript_filename, req->path->s);
    misc.setenv("SCRIPT_FILENAME", lkscript_filename->s, 1);
    lkstring.lk_string_free(lkscript_filename);

    misc.setenv("REQUEST_METHOD", req->method->s, 1);
    misc.setenv("SCRIPT_NAME", req->path->s, 1);
    misc.setenv("REQUEST_URI", req->uri->s, 1);
    misc.setenv("QUERY_STRING", req->querystring->s, 1);

    char *content_type = lk_stringtable_get(req->headers, "Content-Type");
    if (content_type == NULL) {
        content_type = "";
    }
    misc.setenv("CONTENT_TYPE", content_type, 1);

    char content_length[10];
    snprintf(content_length, sizeof(content_length), "%ld", req->body->bytes_len);
    content_length[sizeof(content_length)-1] = '\0';
    misc.setenv("CONTENT_LENGTH", content_length, 1);

    misc.setenv("REMOTE_ADDR", ctx->client_ipaddr->s, 1);
    char portstr[10];
    snprintf(portstr, sizeof(portstr), "%d", ctx->client_port);
    misc.setenv("REMOTE_PORT", portstr, 1);
}

void read_request(LKHttpServer *server, lkcontext.LKContext *ctx) {
    int z = 0;

    while (1) {
        if (!ctx->reqparser->head_complete) {
            z = lk_socketreader_readline(ctx->sr, ctx->req_line);
            if (z == lknet.Z_ERR) {
                lklib.lk_print_err("lksocketreader_readline()");
                break;
            }
            lk_httprequestparser_parse_line(ctx->reqparser, ctx->req_line, ctx->req);
        } else {
            z = lk_socketreader_recv(ctx->sr, ctx->req_buf);
            if (z == lknet.Z_ERR) {
                lklib.lk_print_err("lksocketreader_readbytes()");
                break;
            }
            lk_httprequestparser_parse_bytes(ctx->reqparser, ctx->req_buf, ctx->req);
        }
        // No more data coming in.
        if (ctx->sr->sockclosed) {
            ctx->reqparser->body_complete = 1;
        }
        if (ctx->reqparser->body_complete) {
            FD_CLR_READ(ctx->selectfd, server);
            shutdown(ctx->selectfd, SHUT_RD);
            process_request(server, ctx);
            break;
        }
        if (z != lknet.Z_OPEN) {
            break;
        }
    }
}

// Send cgi_inputbuf input bytes to cgi program stdin set in selectfd.
void write_cgi_input(LKHttpServer *server, lkcontext.LKContext *ctx) {
    assert(ctx->cgi_inputbuf != NULL);
    int z = lk_write_all_file(ctx->selectfd, ctx->cgi_inputbuf);
    if (z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lklib.lk_print_err("write_cgi_input lk_write_all_file()");
        z = terminate_fd(ctx->cgifd, FD_WRITE, server);
        if (z == 0) {
            ctx->cgifd = 0;
        }
        remove_selectfd_context(&server->ctxhead, ctx->selectfd);
        return;
    }
    if (z == lknet.Z_EOF) {
        // Completed writing input bytes.
        FD_CLR_WRITE(ctx->selectfd, server);
        shutdown(ctx->selectfd, SHUT_WR);
        remove_selectfd_context(&server->ctxhead, ctx->selectfd);
    }
}

// Read cgi output to cgi_outputbuf.
void read_cgi_output(LKHttpServer *server, lkcontext.LKContext *ctx) {
    int z = lk_read_all_file(ctx->selectfd, ctx->cgi_outputbuf);
    if (z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lklib.lk_print_err("lk_read_all_file()");
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

    parse_cgi_output(ctx->cgi_outputbuf, ctx->resp);
    process_response(server, ctx);
}

void process_request(LKHttpServer *server, lkcontext.LKContext *ctx) {
    char *hostname = lk_stringtable_get(ctx->req->headers, "Host");
    lkhostconfig.LKHostConfig *hc = lk_config_find_hostconfig(server->cfg, hostname);
    if (hc == NULL) {
        process_error_response(server, ctx, 404, "LittleKitten webserver: hostconfig not found.");
        return;
    }

    // Forward request to proxyhost if proxyhost specified.
    if (hc->proxyhost->s_len > 0) {
        serve_proxy(server, ctx, hc->proxyhost->s);
        return;
    }

    if (hc->homedir->s_len == 0) {
        process_error_response(server, ctx, 404, "LittleKitten webserver: hostconfig homedir not specified.");
        return;
    }

    // Replace path with any matching alias.
    char *match = lk_stringtable_get(hc->aliases, ctx->req->path->s);
    if (match != NULL) {
        lkstring.lk_string_assign(ctx->req->path, match);
    }

    // Run cgi script if uri falls under cgidir
    if (hc->cgidir->s_len > 0 && lkstring.lk_string_starts_with(ctx->req->path, hc->cgidir->s)) {
        serve_cgi(server, ctx, hc);
        return;
    }

    serve_files(server, ctx, hc);
    process_response(server, ctx);
}

// Generate an http response to an http request.
bool POSTTEST = true;
void serve_files(LKHttpServer *server, lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    int z;
    char *html_error_start = 
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Error response</title></head>\n"
       "<body><h1>Error response</h1>\n";
    char *html_error_end =
       "</body></html>\n";

    request.LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;
    lkstring.LKString *method = req->method;
    lkstring.LKString *path = req->path;

    if (lkstring.lk_string_sz_equal(method, "GET") || lkstring.lk_string_sz_equal(method, "HEAD")) {
        // For root, default to index.html, ...
        if (path->s_len == 0) {
            char *default_files[] = {"/index.html", "/index.htm", "/default.html", "/default.htm"};
            for (int i=0; i < nelem(default_files); i++) {
                z = read_path_file(hc->homedir_abspath->s, default_files[i], resp->body);
                if (z >= 0) {
                    lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/html");
                    break;
                }
                // Update path with default file for File not found error message.
                lkstring.lk_string_assign(path, default_files[i]);
            }
        } else {
            z = read_path_file(hc->homedir_abspath->s, path->s, resp->body);
            const char *content_type = mime.lookup(fileext(path->s));
            if (content_type == NULL) {
                content_type = "text/plain";
            }
            lknet.lk_httpresponse_add_header(resp, "Content-Type", content_type);
        }
        if (z == -1) {
            // path not found
            resp->status = 404;
            lk_string_assign_sprintf(resp->statustext, "File not found '%s'", path->s);
            lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
            lkbuffer.lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", path->s);
        }
        return;
    }
    if (POSTTEST) {
        if (lkstring.lk_string_sz_equal(method, "POST")) {
            char *html_start =
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head><title>Little Kitten Sample Response</title></head>\n"
            "<body>\n";
            char *html_end =
            "</body></html>\n";

            lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/html");
            lkbuffer.lk_buffer_append(resp->body, html_start, strlen(html_start));
            lkbuffer.lk_buffer_append_sz(resp->body, "<pre>\n");
            lkbuffer.lk_buffer_append(resp->body, req->body->bytes, req->body->bytes_len);
            lkbuffer.lk_buffer_append_sz(resp->body, "\n</pre>\n");
            lkbuffer.lk_buffer_append(resp->body, html_end, strlen(html_end));
            return;
        }
    }

    resp->status = 501;
    lk_string_assign_sprintf(resp->statustext, "Unsupported method ('%s')", method);

    lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/html");
    lkbuffer.lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
    lkbuffer.lk_buffer_append_sprintf(resp->body, "<p>Error code %d.</p>\n", resp->status);
    lkbuffer.lk_buffer_append_sprintf(resp->body, "<p>Message: Unsupported method ('%s').</p>\n", resp->statustext->s);
    lkbuffer.lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
}

void serve_cgi(LKHttpServer *server, lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    request.LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;
    char *path = req->path->s;

    lkstring.LKString *cgifile = lk_string_new(hc->homedir_abspath->s);
    lk_string_append(cgifile, req->path->s);

    // Expand "/../", etc. into real_path.
    char real_path[PATH_MAX];
    char *pz = realpath(cgifile->s, real_path);
    lkstring.lk_string_free(cgifile);

    // real_path should start with cgidir_abspath
    // real_path file should exist
    if (pz == NULL || strncmp(real_path, hc->cgidir_abspath->s, hc->cgidir_abspath->s_len) || !fileutil.file_exists(real_path)) {
        resp->status = 404;
        lk_string_assign_sprintf(resp->statustext, "File not found '%s'", path);
        lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lkbuffer.lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", path);

        process_response(server, ctx);
        return;
    }

    set_cgi_env2(server, ctx, hc);

    // cgi stdout and stderr are streamed to fd_out.
    //$$todo pass any request body to fd_in.
    int fd_in, fd_out;
    int z = lk_popen3(real_path, &fd_in, &fd_out, NULL);
    if (z == -1) {
        resp->status = 500;
        lk_string_assign_sprintf(resp->statustext, "Server error '%s'", strerror(errno));
        lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lkbuffer.lk_buffer_append_sprintf(resp->body, "Server error '%s'\n", strerror(errno));
        process_response(server, ctx);
        return;
    }

    close(fd_in);

    // Read cgi output in select()
    ctx->selectfd = fd_out;
    ctx->cgifd = fd_out;
    ctx->type = CTX_READ_CGI_OUTPUT;
    ctx->cgi_outputbuf = lk_buffer_new(0);
    FD_SET_READ(ctx->selectfd, server);

    // If req is POST with body, pass it to cgi process stdin.
    if (req->body->bytes_len > 0) {
        lkcontext.LKContext *ctx_in = lk_context_new();
        add_context(&server->ctxhead, ctx_in);

        ctx_in->selectfd = fd_in;
        ctx_in->cgifd = fd_in;
        ctx_in->clientfd = ctx->clientfd;
        ctx_in->type = CTX_WRITE_CGI_INPUT;

        ctx_in->cgi_inputbuf = lk_buffer_new(0);
        lkbuffer.lk_buffer_append(ctx_in->cgi_inputbuf, req->body->bytes, req->body->bytes_len);

        FD_SET_WRITE(ctx_in->selectfd, server);
    }
}

void process_response(LKHttpServer *server, lkcontext.LKContext *ctx) {
    request.LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;

    lk_httpresponse_finalize(resp);

    // Clear response body on HEAD request.
    if (lkstring.lk_string_sz_equal(req->method, "HEAD")) {
        lk_buffer_clear(resp->body);
    }

    char time_str[TIME_STRING_SIZE];
    get_localtime_string(time_str, sizeof(time_str));
    printf("%s [%s] \"%s %s %s\" %d\n", 
        ctx->client_ipaddr->s, time_str,
        req->method->s, req->uri->s, resp->version->s,
        resp->status);
    if (resp->status >= 500 && resp->status < 600 && resp->statustext->s_len > 0) {
        printf("%s [%s] %d - %s\n", 
            ctx->client_ipaddr->s, time_str,
            resp->status, resp->statustext->s);
    }

    ctx->selectfd = ctx->clientfd;
    ctx->type = CTX_WRITE_RESP;
    FD_SET_WRITE(ctx->selectfd, server);
    lkreflist.lk_reflist_clear(ctx->buflist);
    lkreflist.lk_reflist_append(ctx->buflist, resp->head);
    lkreflist.lk_reflist_append(ctx->buflist, resp->body);
    return;
}

void process_error_response(LKHttpServer *server, lkcontext.LKContext *ctx, int status, char *msg) {
    LKHttpResponse *resp = ctx->resp;
    resp->status = status;
    lkstring.lk_string_assign(resp->statustext, msg);
    lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
    lkbuffer.lk_buffer_append_sz(resp->body, msg);

    process_response(server, ctx);
}

// Open <home_dir>/<uri> file in nonblocking mode.
// Returns 0 for success, -1 for error.
int open_path_file(char *home_dir, char *path) {
    // full_path = home_dir + path
    // Ex. "/path/to" + "/index.html"
    lkstring.LKString *full_path = lk_string_new(home_dir);
    lk_string_append(full_path, path);

    int z = open(full_path->s, O_RDONLY | O_NONBLOCK);
    lkstring.lk_string_free(full_path);
    return z;
}

// Read <home_dir>/<uri> file into buffer.
// Return number of bytes read or -1 for error.
int read_path_file(char *home_dir, char *path, lkbuffer.LKBuffer *buf) {
    int z;
    // full_path = home_dir + path
    // Ex. "/path/to" + "/index.html"
    lkstring.LKString *full_path = lk_string_new(home_dir);
    lk_string_append(full_path, path);

    // Expand "/../", etc. into real_path.
    char real_path[PATH_MAX];
    char *pz = realpath(full_path->s, real_path);
    // real_path should start with home_dir
    if (pz == NULL || strncmp(real_path, home_dir, strlen(home_dir))) {
        lkstring.lk_string_free(full_path);
        z = -1;
        errno = EPERM;
        return z;
    }

    z = lk_readfile(real_path, buf);
    lkstring.lk_string_free(full_path);
    return z;
}

int is_valid_http_method(char *method) {
    if (method == NULL) {
        return 0;
    }

    if (!strings.casecmp(method, "GET")      ||
        !strings.casecmp(method, "POST")     ||
        !strings.casecmp(method, "PUT")      ||
        !strings.casecmp(method, "DELETE")   ||
        !strings.casecmp(method, "HEAD"))  {
        return 1;
    }

    return 0;
}

// Return ptr to start of file extension within filepath.
// Ex. "path/to/index.html" returns "index.html"
char *fileext(char *filepath) {
    int filepath_len = strlen(filepath);
    // filepath of "" returns ext of "".
    if (filepath_len == 0) {
        return filepath;
    }

    char *p = filepath + strlen(filepath) - 1;
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
        lklib.lk_print_err("write_response lk_buflist_write_all()");
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
        lklib.lk_print_err("lk_open_connect_socket()");
        process_error_response(server, ctx, 500, "Error opening proxy socket.");
        return;
    }

    // hack
    int proxyfd = proxyconn->fd;

    lknet.lk_httprequest_finalize(ctx->req);
    ctx->proxyfd = proxyfd;
    ctx->selectfd = proxyfd;
    ctx->type = CTX_PROXY_WRITE_REQ;
    FD_SET_WRITE(proxyfd, server);
    lkreflist.lk_reflist_clear(ctx->buflist);
    lkreflist.lk_reflist_append(ctx->buflist, ctx->req->head);
    lkreflist.lk_reflist_append(ctx->buflist, ctx->req->body);
}

void write_proxy_request(LKHttpServer *server, lkcontext.LKContext *ctx) {
    int z = lk_buflist_write_all(ctx->selectfd, FD_SOCK, ctx->buflist);
    if (z == lknet.Z_BLOCK) {
        return;
    }
    if (z == lknet.Z_ERR) {
        lklib.lk_print_err("write_proxy_request lk_buflist_write_all");
        z = terminate_sock(ctx->proxyfd, FD_WRITE, server);
        if (z == 0) {
            ctx->proxyfd = 0;
        }
        process_error_response(server, ctx, 500, "Error forwarding request to proxy.");
        return;
    }
    if (z == lknet.Z_EOF) {
        // Completed sending http request.
        FD_CLR_WRITE(ctx->selectfd, server);
        shutdown(ctx->selectfd, SHUT_WR);

        // Pipe proxy response from ctx->proxyfd to ctx->clientfd
        ctx->type = CTX_PROXY_PIPE_RESP;
        ctx->proxy_respbuf = lk_buffer_new(0);
        FD_SET_READ(ctx->selectfd, server);
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
        lklib.lk_print_err("pipe_proxy_response lk_pipe_all()");
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

    request.LKHttpRequest *req = ctx->req;
    char time_str[TIME_STRING_SIZE];
    get_localtime_string(time_str, sizeof(time_str));
    printf("%s [%s] \"%s %s\" --> proxyhost\n",
        ctx->client_ipaddr->s, time_str, req->method->s, req->uri->s);

    // Completed sending proxy response.
    terminate_client_session(server, ctx);
}

enum {
    FD_READ,
    FD_WRITE,
    FD_READWRITE
}; // fd_action

// Clear fd from select()'s, shutdown, and close.
int terminate_fd(int fd, int fd_action, LKHttpServer *server) {
    int z;
    if (fd_action == FD_READ || fd_action == FD_READWRITE) {
        FD_CLR_READ(fd, server);
    }
    if (fd_action == FD_WRITE || fd_action == FD_READWRITE) {
        FD_CLR_WRITE(fd, server);
    }
    z = close(fd);
    if (z == -1) {
        lklib.lk_print_err("close()");
    }
    return z;
}

int terminate_sock(int fd, int fd_action, LKHttpServer *server) {
    if (fd_action == FD_READ || fd_action == FD_READWRITE) {
        FD_CLR_READ(fd, server);
    }
    if (fd_action == FD_WRITE || fd_action == FD_READWRITE) {
        FD_CLR_WRITE(fd, server);
    }
    int z = close(fd);
    if (z == -1) {
        lklib.lk_print_err("close()");
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


void add_client(LKHttpServer *server) {
    // hack
    int clientfd = client->sock;
    FD_SET_READ(clientfd, server);
    lkcontext.LKContext *ctx = lkcontext.create_initial_context(clientfd);
    lkcontext.add_new_client_context(&server->ctxhead, ctx);
}

void read_client(LKHttpServer *server, net.net_t *client) {
    // hack
    //printf("read fd %d\n", i);
    int selectfd = client->fd;

    lkcontext.LKContext *ctx = lkcontext.match_select_ctx(server->ctxhead, selectfd);
    if (ctx == NULL) {
        printf("read selectfd %d not in ctx list\n", selectfd);
        terminate_sock(selectfd, FD_READ, server);
        continue;
    }
    if (ctx->type == CTX_READ_REQ) {
        read_request(server, ctx);
    } else if (ctx->type == CTX_READ_CGI_OUTPUT) {
        read_cgi_output(server, ctx);
    } else if (ctx->type == CTX_PROXY_PIPE_RESP) {
        pipe_proxy_response(server, ctx);
    } else {
        printf("read selectfd %d with unknown ctx type %d\n", selectfd, ctx->type);
    }
}

void write_client(LKHttpServer *server, net.net_t *client) {
    // hack
    int selectfd = client->fd;
    lkcontext.LKContext *ctx = lkcontext.match_select_ctx(server->ctxhead, selectfd);
    if (ctx == NULL) {
        printf("write selectfd %d not in ctx list\n", selectfd);
        terminate_sock(selectfd, FD_WRITE, server);
        continue;
    }

    if (ctx->type == CTX_WRITE_RESP) {
        assert(ctx->resp != NULL);
        assert(ctx->resp->head != NULL);
        write_response(server, ctx);
    } else if (ctx->type == CTX_WRITE_CGI_INPUT) {
        write_cgi_input(server, ctx);
    } else if (ctx->type == CTX_PROXY_WRITE_REQ) {
        assert(ctx->req != NULL);
        assert(ctx->req->head != NULL);
        write_proxy_request(server, ctx);
    } else {
        printf("write selectfd %d with unknown ctx type %d\n", selectfd, ctx->type);
    }
}

// localtime in server format: 11/Mar/2023 14:05:46
void get_localtime_string(char *buf, size_t size) {
    if (!time.time_format(buf, size, "%d/%b/%Y %H:%M:%S")) {
        snprintf(buf, size, "%s", "failed to get time: %s", strerror(errno));
    }
}

// Pipe all available nonblocking readfd bytes into writefd.
// Uses buf as buffer for queued up bytes waiting to be written.
// Returns one of the following:
//    0 (lknet.Z_EOF) for read/write complete.
//    1 (lknet.Z_OPEN) for writefd socket open
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