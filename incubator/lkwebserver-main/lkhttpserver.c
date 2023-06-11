#import net
#import lkconfig.c
#import lkalloc.c
#import mime

pub typedef {
    lkconfig.LKConfig *cfg;
    LKContext *ctxhead;
    fd_set readfds;
    fd_set writefds;
    int maxfd;
    // The listening connection.
    net.net_t *listen_conn;
} LKHttpServer;

pub LKHttpServer *lk_httpserver_new(lkconfig.LKConfig *cfg) {
    LKHttpServer *server = lkalloc.lk_malloc(sizeof(LKHttpServer), "lk_httpserver_new");
    server->cfg = cfg;
    server->ctxhead = NULL;
    server->maxfd = 0;
    return server;
}

pub void lk_httpserver_free(LKHttpServer *server) {
    lkconfig.lk_config_free(server->cfg);

    // Free ctx linked list
    LKContext *ctx = server->ctxhead;
    while (ctx != NULL) {
        LKContext *ptmp = ctx;
        ctx = ctx->next;
        lk_context_free(ptmp);
    }

    memset(server, 0, sizeof(LKHttpServer));
    lk_free(server);
}

void FD_SET_READ(int fd, LKHttpServer *server) {
    FD_SET(fd, &server->readfds);
    if (fd > server->maxfd) {
        server->maxfd = fd;
    }
}
void FD_SET_WRITE(int fd, LKHttpServer *server) {
    FD_SET(fd, &server->writefds);
    if (fd > server->maxfd) {
        server->maxfd = fd;
    }
}
void FD_CLR_READ(int fd, LKHttpServer *server) {
    FD_CLR(fd, &server->readfds);
}
void FD_CLR_WRITE(int fd, LKHttpServer *server) {
    FD_CLR(fd, &server->writefds);
}

pub int lk_httpserver_serve(lkconfig.LKConfig *cfg) {
    LKHttpServer *httpserver = lkhttpserver.lk_httpserver_new(cfg);

    char *addr = strutil.newstr("%s:%s", cfg->serverhost->s, cfg->port->s);
    int httpserver->listen_conn = net.net_listen("tcp", addr);
    if (!httpserver->listen_conn) {
        free(addr);
        return -1;
    }
    printf("Serving HTTP at %s\n", addr);
    free(addr);

    clearenv();
    set_cgi_env1(server);

    FD_ZERO(&server->readfds);
    FD_ZERO(&server->writefds);

    FD_SET(s0, &server->readfds);
    server->maxfd = s0;

    while (1) {
        // New client connection?
        if (net.has_data(server->listen_conn)) {
            net_t *client = net.net_accept(server->listen_conn);
            if (!client) {
                lk_print_err("accept()");
                continue;
            }

            // hack
            int clientfd = client->sock;

            // Add new client socket to list of read sockets.
            FD_SET_READ(clientfd, server);
            LKContext *ctx = create_initial_context(clientfd, &sa);
            add_new_client_context(&server->ctxhead, ctx);
            continue;
        }

        // readfds contain the master list of read sockets
        fd_set cur_readfds = server->readfds;
        fd_set cur_writefds = server->writefds;
        z = select(server->maxfd+1, &cur_readfds, &cur_writefds, NULL, NULL);
        if (z == -1 && errno == EINTR) {
            continue;
        }
        if (z == -1) {
            lk_print_err("select()");
            return z;
        }
        if (z == 0) {
            // timeout returned
            continue;
        }

        // fds now contain list of clients with data available to be read.
        for (int i=0; i <= server->maxfd; i++) {
            if (FD_ISSET(i, &cur_readfds)) {
                //printf("read fd %d\n", i);
                int selectfd = i;
                LKContext *ctx = match_select_ctx(server->ctxhead, selectfd);
                if (ctx == NULL) {
                    printf("read selectfd %d not in ctx list\n", selectfd);
                    terminate_fd(selectfd, FD_SOCK, FD_READ, server);
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
            } else if (FD_ISSET(i, &cur_writefds)) {
                //printf("write fd %d\n", i);

                int selectfd = i;
                LKContext *ctx = match_select_ctx(server->ctxhead, selectfd);
                if (ctx == NULL) {
                    printf("write selectfd %d not in ctx list\n", selectfd);
                    terminate_fd(selectfd, FD_SOCK, FD_WRITE, server);
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
        }
    } // while (1)

    return 0;
}

// Sets the cgi environment variables that stay the same across http requests.
void set_cgi_env1(LKHttpServer *server) {
    int z;
    lkconfig.LKConfig *cfg = server->cfg;

    char hostname[LK_BUFSIZE_SMALL];
    z = gethostname(hostname, sizeof(hostname)-1);
    if (z == -1) {
        lk_print_err("gethostname()");
        hostname[0] = '\0';
    }
    hostname[sizeof(hostname)-1] = '\0';
    
    setenv("SERVER_NAME", hostname, 1);
    setenv("SERVER_SOFTWARE", "littlekitten/0.1", 1);
    setenv("SERVER_PROTOCOL", "HTTP/1.0", 1);
    setenv("SERVER_PORT", cfg->port->s, 1);

}

// Sets the cgi environment variables that vary for each http request.
void set_cgi_env2(LKHttpServer *server, LKContext *ctx, LKHostConfig *hc) {
    LKHttpRequest *req = ctx->req;

    setenv("DOCUMENT_ROOT", hc->homedir_abspath->s, 1);

    char *http_user_agent = lk_stringtable_get(req->headers, "User-Agent");
    if (!http_user_agent) http_user_agent = "";
    setenv("HTTP_USER_AGENT", http_user_agent, 1);

    char *http_host = lk_stringtable_get(req->headers, "Host");
    if (!http_host) http_host = "";
    setenv("HTTP_HOST", http_host, 1);

    LKString *lkscript_filename = lk_string_new(hc->homedir_abspath->s);
    lk_string_append(lkscript_filename, req->path->s);
    setenv("SCRIPT_FILENAME", lkscript_filename->s, 1);
    lkstring.lk_string_free(lkscript_filename);

    setenv("REQUEST_METHOD", req->method->s, 1);
    setenv("SCRIPT_NAME", req->path->s, 1);
    setenv("REQUEST_URI", req->uri->s, 1);
    setenv("QUERY_STRING", req->querystring->s, 1);

    char *content_type = lk_stringtable_get(req->headers, "Content-Type");
    if (content_type == NULL) {
        content_type = "";
    }
    setenv("CONTENT_TYPE", content_type, 1);

    char content_length[10];
    snprintf(content_length, sizeof(content_length), "%ld", req->body->bytes_len);
    content_length[sizeof(content_length)-1] = '\0';
    setenv("CONTENT_LENGTH", content_length, 1);

    setenv("REMOTE_ADDR", ctx->client_ipaddr->s, 1);
    char portstr[10];
    snprintf(portstr, sizeof(portstr), "%d", ctx->client_port);
    setenv("REMOTE_PORT", portstr, 1);
}

void read_request(LKHttpServer *server, LKContext *ctx) {
    int z = 0;

    while (1) {
        if (!ctx->reqparser->head_complete) {
            z = lk_socketreader_readline(ctx->sr, ctx->req_line);
            if (z == Z_ERR) {
                lk_print_err("lksocketreader_readline()");
                break;
            }
            lk_httprequestparser_parse_line(ctx->reqparser, ctx->req_line, ctx->req);
        } else {
            z = lk_socketreader_recv(ctx->sr, ctx->req_buf);
            if (z == Z_ERR) {
                lk_print_err("lksocketreader_readbytes()");
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
        if (z != Z_OPEN) {
            break;
        }
    }
}

// Send cgi_inputbuf input bytes to cgi program stdin set in selectfd.
void write_cgi_input(LKHttpServer *server, LKContext *ctx) {
    assert(ctx->cgi_inputbuf != NULL);
    int z = lk_write_all_file(ctx->selectfd, ctx->cgi_inputbuf);
    if (z == Z_BLOCK) {
        return;
    }
    if (z == Z_ERR) {
        lk_print_err("write_cgi_input lk_write_all_file()");
        z = terminate_fd(ctx->cgifd, FD_FILE, FD_WRITE, server);
        if (z == 0) {
            ctx->cgifd = 0;
        }
        remove_selectfd_context(&server->ctxhead, ctx->selectfd);
        return;
    }
    if (z == Z_EOF) {
        // Completed writing input bytes.
        FD_CLR_WRITE(ctx->selectfd, server);
        shutdown(ctx->selectfd, SHUT_WR);
        remove_selectfd_context(&server->ctxhead, ctx->selectfd);
    }
}

// Read cgi output to cgi_outputbuf.
void read_cgi_output(LKHttpServer *server, LKContext *ctx) {
    int z = lk_read_all_file(ctx->selectfd, ctx->cgi_outputbuf);
    if (z == Z_BLOCK) {
        return;
    }
    if (z == Z_ERR) {
        lk_print_err("lk_read_all_file()");
        z = terminate_fd(ctx->cgifd, FD_FILE, FD_READ, server);
        if (z == 0) {
            ctx->cgifd = 0;
        }
        process_error_response(server, ctx, 500, "Error processing CGI output.");
        return;
    }

    // EOF - finished reading cgi output.
    assert(z == 0);

    // Remove cgi output from read list.
    z = terminate_fd(ctx->cgifd, FD_FILE, FD_READ, server);
    if (z == 0) {
        ctx->cgifd = 0;
    }

    parse_cgi_output(ctx->cgi_outputbuf, ctx->resp);
    process_response(server, ctx);
}

void process_request(LKHttpServer *server, LKContext *ctx) {
    char *hostname = lk_stringtable_get(ctx->req->headers, "Host");
    LKHostConfig *hc = lk_config_find_hostconfig(server->cfg, hostname);
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
        lk_string_assign(ctx->req->path, match);
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
#define POSTTEST
void serve_files(LKHttpServer *server, LKContext *ctx, LKHostConfig *hc) {
    int z;
    static char *html_error_start = 
       "<!DOCTYPE html>\n"
       "<html>\n"
       "<head><title>Error response</title></head>\n"
       "<body><h1>Error response</h1>\n";
    static char *html_error_end =
       "</body></html>\n";

    LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;
    LKString *method = req->method;
    LKString *path = req->path;

    if (lk_string_sz_equal(method, "GET") || lk_string_sz_equal(method, "HEAD")) {
        // For root, default to index.html, ...
        if (path->s_len == 0) {
            char *default_files[] = {"/index.html", "/index.htm", "/default.html", "/default.htm"};
            for (int i=0; i < sizeof(default_files) / sizeof(char *); i++) {
                z = read_path_file(hc->homedir_abspath->s, default_files[i], resp->body);
                if (z >= 0) {
                    lk_httpresponse_add_header(resp, "Content-Type", "text/html");
                    break;
                }
                // Update path with default file for File not found error message.
                lk_string_assign(path, default_files[i]);
            }
        } else {
            z = read_path_file(hc->homedir_abspath->s, path->s, resp->body);
            const char *content_type = mime.lookup(fileext(path->s));
            if (content_type == NULL) {
                content_type = "text/plain";
            }
            lk_httpresponse_add_header(resp, "Content-Type", content_type);
        }
        if (z == -1) {
            // path not found
            resp->status = 404;
            lk_string_assign_sprintf(resp->statustext, "File not found '%s'", path->s);
            lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
            lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", path->s);
        }
        return;
    }
#ifdef POSTTEST
    if (lk_string_sz_equal(method, "POST")) {
        static char *html_start =
           "<!DOCTYPE html>\n"
           "<html>\n"
           "<head><title>Little Kitten Sample Response</title></head>\n"
           "<body>\n";
        static char *html_end =
           "</body></html>\n";

        lk_httpresponse_add_header(resp, "Content-Type", "text/html");
        lk_buffer_append(resp->body, html_start, strlen(html_start));
        lk_buffer_append_sz(resp->body, "<pre>\n");
        lk_buffer_append(resp->body, req->body->bytes, req->body->bytes_len);
        lk_buffer_append_sz(resp->body, "\n</pre>\n");
        lk_buffer_append(resp->body, html_end, strlen(html_end));
        return;
    }
#endif

    resp->status = 501;
    lk_string_assign_sprintf(resp->statustext, "Unsupported method ('%s')", method);

    lk_httpresponse_add_header(resp, "Content-Type", "text/html");
    lk_buffer_append(resp->body, html_error_start, strlen(html_error_start));
    lk_buffer_append_sprintf(resp->body, "<p>Error code %d.</p>\n", resp->status);
    lk_buffer_append_sprintf(resp->body, "<p>Message: Unsupported method ('%s').</p>\n", resp->statustext->s);
    lk_buffer_append(resp->body, html_error_end, strlen(html_error_end));
}

void serve_cgi(LKHttpServer *server, LKContext *ctx, LKHostConfig *hc) {
    LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;
    char *path = req->path->s;

    LKString *cgifile = lk_string_new(hc->homedir_abspath->s);
    lk_string_append(cgifile, req->path->s);

    // Expand "/../", etc. into real_path.
    char real_path[PATH_MAX];
    char *pz = realpath(cgifile->s, real_path);
    lkstring.lk_string_free(cgifile);

    // real_path should start with cgidir_abspath
    // real_path file should exist
    if (pz == NULL || strncmp(real_path, hc->cgidir_abspath->s, hc->cgidir_abspath->s_len) || !lk_file_exists(real_path)) {
        resp->status = 404;
        lk_string_assign_sprintf(resp->statustext, "File not found '%s'", path);
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "File not found '%s'\n", path);

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
        lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
        lk_buffer_append_sprintf(resp->body, "Server error '%s'\n", strerror(errno));
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
        LKContext *ctx_in = lk_context_new();
        add_context(&server->ctxhead, ctx_in);

        ctx_in->selectfd = fd_in;
        ctx_in->cgifd = fd_in;
        ctx_in->clientfd = ctx->clientfd;
        ctx_in->type = CTX_WRITE_CGI_INPUT;

        ctx_in->cgi_inputbuf = lk_buffer_new(0);
        lk_buffer_append(ctx_in->cgi_inputbuf, req->body->bytes, req->body->bytes_len);

        FD_SET_WRITE(ctx_in->selectfd, server);
    }
}

void process_response(LKHttpServer *server, LKContext *ctx) {
    LKHttpRequest *req = ctx->req;
    LKHttpResponse *resp = ctx->resp;

    lk_httpresponse_finalize(resp);

    // Clear response body on HEAD request.
    if (lk_string_sz_equal(req->method, "HEAD")) {
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
    lk_reflist_clear(ctx->buflist);
    lk_reflist_append(ctx->buflist, resp->head);
    lk_reflist_append(ctx->buflist, resp->body);
    return;
}

void process_error_response(LKHttpServer *server, LKContext *ctx, int status, char *msg) {
    LKHttpResponse *resp = ctx->resp;
    resp->status = status;
    lk_string_assign(resp->statustext, msg);
    lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
    lk_buffer_append_sz(resp->body, msg);

    process_response(server, ctx);
}

// Open <home_dir>/<uri> file in nonblocking mode.
// Returns 0 for success, -1 for error.
int open_path_file(char *home_dir, char *path) {
    // full_path = home_dir + path
    // Ex. "/path/to" + "/index.html"
    LKString *full_path = lk_string_new(home_dir);
    lk_string_append(full_path, path);

    int z = open(full_path->s, O_RDONLY | O_NONBLOCK);
    lkstring.lk_string_free(full_path);
    return z;
}

// Read <home_dir>/<uri> file into buffer.
// Return number of bytes read or -1 for error.
int read_path_file(char *home_dir, char *path, LKBuffer *buf) {
    int z;
    // full_path = home_dir + path
    // Ex. "/path/to" + "/index.html"
    LKString *full_path = lk_string_new(home_dir);
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

    if (!strcasecmp(method, "GET")      ||
        !strcasecmp(method, "POST")     || 
        !strcasecmp(method, "PUT")      || 
        !strcasecmp(method, "DELETE")   ||
        !strcasecmp(method, "HEAD"))  {
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

void write_response(LKHttpServer *server, LKContext *ctx) {
    int z = lk_buflist_write_all(ctx->selectfd, FD_SOCK, ctx->buflist);
    if (z == Z_BLOCK) {
        return;
    }
    if (z == Z_ERR) {
        lk_print_err("write_response lk_buflist_write_all()");
        terminate_client_session(server, ctx);
        return;
    }
    if (z == Z_EOF) {
        // Completed sending http response.
        terminate_client_session(server, ctx);
    }
}

void serve_proxy(LKHttpServer *server, LKContext *ctx, char *targethost) {
    int proxyfd = lk_open_connect_socket(targethost, "", NULL);
    if (proxyfd == -1) {
        lk_print_err("lk_open_connect_socket()");
        process_error_response(server, ctx, 500, "Error opening proxy socket.");
        return;
    }

    lk_httprequest_finalize(ctx->req);
    ctx->proxyfd = proxyfd;
    ctx->selectfd = proxyfd;
    ctx->type = CTX_PROXY_WRITE_REQ;
    FD_SET_WRITE(proxyfd, server);
    lk_reflist_clear(ctx->buflist);
    lk_reflist_append(ctx->buflist, ctx->req->head);
    lk_reflist_append(ctx->buflist, ctx->req->body);
}

void write_proxy_request(LKHttpServer *server, LKContext *ctx) {
    int z = lk_buflist_write_all(ctx->selectfd, FD_SOCK, ctx->buflist);
    if (z == Z_BLOCK) {
        return;
    }
    if (z == Z_ERR) {
        lk_print_err("write_proxy_request lk_buflist_write_all");
        z = terminate_fd(ctx->proxyfd, FD_SOCK, FD_WRITE, server);
        if (z == 0) {
            ctx->proxyfd = 0;
        }
        process_error_response(server, ctx, 500, "Error forwarding request to proxy.");
        return;
    }
    if (z == Z_EOF) {
        // Completed sending http request.
        FD_CLR_WRITE(ctx->selectfd, server);
        shutdown(ctx->selectfd, SHUT_WR);

        // Pipe proxy response from ctx->proxyfd to ctx->clientfd
        ctx->type = CTX_PROXY_PIPE_RESP;
        ctx->proxy_respbuf = lk_buffer_new(0);
        FD_SET_READ(ctx->selectfd, server);
    }
}

// Similar to lk_write_all(), but sending buflist buf's sequentially.
int lk_buflist_write_all(int fd, FDType fd_type, LKRefList *buflist) {
    if (buflist->items_cur >= buflist->items_len) {
        return Z_EOF;
    }

    LKBuffer *buf = buflist->items[buflist->items_cur];
    int z = lk_write_all(fd, fd_type, buf);
    if (z == Z_EOF) {
        buflist->items_cur++;
        z = Z_OPEN;
    }
    return z;
}

void pipe_proxy_response(LKHttpServer *server, LKContext *ctx) {
    int z = lk_pipe_all(ctx->proxyfd, ctx->clientfd, FD_SOCK, ctx->proxy_respbuf);
    if (z == Z_OPEN || z == Z_BLOCK) {
        return;
    }
    if (z == Z_ERR) {
        lk_print_err("pipe_proxy_response lk_pipe_all()");
        z = terminate_fd(ctx->proxyfd, FD_SOCK, FD_READ, server);
        if (z == 0) {
            ctx->proxyfd = 0;
        }
        process_error_response(server, ctx, 500, "Error reading/writing proxy response.");
        return;
    }

    // EOF - finished reading/writing proxy response.
    assert(z == Z_EOF);

    // Remove proxy from read list.
    z = terminate_fd(ctx->proxyfd, FD_SOCK, FD_READ, server);
    if (z == 0) {
        ctx->proxyfd = 0;
    }

    LKHttpRequest *req = ctx->req;
    char time_str[TIME_STRING_SIZE];
    get_localtime_string(time_str, sizeof(time_str));
    printf("%s [%s] \"%s %s\" --> proxyhost\n",
        ctx->client_ipaddr->s, time_str, req->method->s, req->uri->s);

    // Completed sending proxy response.
    terminate_client_session(server, ctx);
}

//$$ read_proxy_response() and write_response() were
//   replaced by pipe_proxy_response().
#if 0
void read_proxy_response(LKHttpServer *server, LKContext *ctx) {
    int z = lk_read_all_sock(ctx->selectfd, ctx->proxy_respbuf);
    if (z == Z_BLOCK) {
        return;
    }
    if (z == Z_ERR) {
        lk_print_err("lk_read_all_sock()");
        z = terminate_fd(ctx->proxyfd, FD_SOCK, FD_READ, server);
        if (z == 0) {
            ctx->proxyfd = 0;
        }
        process_error_response(server, ctx, 500, "Error reading proxy response.");
        return;
    }

    // EOF - finished reading proxy response.
    assert(z == 0);

    // Remove proxy from read list.
    z = terminate_fd(ctx->proxyfd, FD_SOCK, FD_READ, server);
    if (z == 0) {
        ctx->proxyfd = 0;
    }

    LKHttpRequest *req = ctx->req;
    char time_str[TIME_STRING_SIZE];
    get_localtime_string(time_str, sizeof(time_str));
    printf("%s [%s] \"%s %s\" --> proxyhost\n",
        ctx->client_ipaddr->s, time_str, req->method->s, req->uri->s);

    ctx->type = CTX_PROXY_WRITE_RESP;
    ctx->selectfd = ctx->clientfd;
    FD_SET_WRITE(ctx->clientfd, server);
}

void write_proxy_response(LKHttpServer *server, LKContext *ctx) {
    assert(ctx->proxy_respbuf != NULL);
    int z = lk_write_all_sock(ctx->selectfd, ctx->proxy_respbuf);
    if (z == Z_BLOCK) {
        return;
    }
    if (z == Z_ERR) {
        lk_print_err("write_proxy_response lk_write_all_sock()");
        process_error_response(server, ctx, 500, "Error sending proxy response.");
        return;
    }
    if (z == Z_EOF) {
        // Completed sending http response.
        terminate_client_session(server, ctx);
    }
}
#endif

// Clear fd from select()'s, shutdown, and close.
int terminate_fd(int fd, FDType fd_type, FDAction fd_action, LKHttpServer *server) {
    int z;
    if (fd_action == FD_READ || fd_action == FD_READWRITE) {
        FD_CLR_READ(fd, server);
    }
    if (fd_action == FD_WRITE || fd_action == FD_READWRITE) {
        FD_CLR_WRITE(fd, server);
    }

    if (fd_type == FD_SOCK) {
        if (fd_action == FD_READ) {
            z = shutdown(fd, SHUT_RD);
        } else if (fd_action == FD_WRITE) {
            z = shutdown(fd, SHUT_WR);
        } else {
            z = shutdown(fd, SHUT_RDWR);
        }
        if (z == -1) {
            //$$ Suppress logging shutdown error because remote sockets close themselves.
            //lk_print_err("terminate_fd shutdown()");
        }
    }
    z = close(fd);
    if (z == -1) {
        lk_print_err("close()");
    }
    return z;
}

// Disconnect from client.
void terminate_client_session(LKHttpServer *server, LKContext *ctx) {
    if (ctx->clientfd) {
        terminate_fd(ctx->clientfd, FD_SOCK, FD_READWRITE, server);
    }
    if (ctx->cgifd) {
        terminate_fd(ctx->cgifd, FD_FILE, FD_READWRITE, server);
    }
    if (ctx->proxyfd) {
        terminate_fd(ctx->proxyfd, FD_SOCK, FD_READWRITE, server);
    }
    // Remove from linked list and free ctx.
    remove_client_context(&server->ctxhead, ctx->clientfd);
}

