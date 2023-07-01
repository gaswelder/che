#import fileutil
#import http
#import mime
#import os/io
#import os/misc
#import strings
#import os/fs

// #import lkbuffer.c
#import lkconfig.c
#import lkcontext.c
#import lkhostconfig.c
#import srvfiles.c
#import llist.c
#import srvstd.c

#define LK_BUFSIZE_SMALL 512
#define TIME_STRING_SIZE 25
#define MAX_CLIENTS 1000

enum {FD_SOCK, FD_FILE};


pub typedef {
    lkconfig.LKConfig *cfg;
    llist.t *contexts;
} server_t;

pub bool lk_httpserver_serve(lkconfig.LKConfig *cfg) {
    server_t server = {
        .cfg = cfg,
        .contexts = llist.new()
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
     * Start listening.
     */
    char *addr = strings.newstr("%s:%s", cfg->serverhost, cfg->port);
    io.handle_t *listener = io.listen("tcp", addr);
    if (!listener) {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        free(addr);
        return false;
    }
    printf("listening at %d\n", listener->fd);
    printf("Serving at http://%s\n", addr);
    free(addr);

    io.pool_t *p = io.newpool();
    if (!p) {
        panic("failed to create a pool");
    }

    /*
     * Poll and process the handles.
     */
    int c = 0;
    while (true) {
        printf("-------------- loop %d --------------\n", c++);
        io.resetpool(p);
        io.add(p, listener, io.READ);
        llist.t *it = llist.next(server.contexts);
        while (it) {
            lkcontext.LKContext *ctx = it->data;
            if (ctx->waithandle) {
                printf("routine waits for %d (%d)\n", ctx->waithandle->fd, ctx->waitfilter);
                io.add(p, ctx->waithandle, ctx->waitfilter);
            }
            it = llist.next(it);
        }

        io.event_t *ev = io.poll(p);
        while (ev->handle) {
            printf("poll result: %d, r=%d, w=%d\n", ev->handle->fd, ev->readable, ev->writable);


            if (ev->handle == listener) {
                printf("accepting\n");
                io.handle_t *conn = io.accept(ev->handle);
                printf("accepted %d\n", conn->fd);
                if (!conn) {
                    fprintf(stderr, "accept failed: %s\n", strerror(errno));
                    panic("!");
                }
                lkcontext.LKContext *ctx = lkcontext.create_initial_context(conn);
                llist.append(server.contexts, ctx);
                execute_context(&server, ctx);
                ev++;
                continue;
            }

            // Find the context that requested this IO event.
            lkcontext.LKContext *ctx = find_context(&server, ev);
            if (!ctx) {
                panic("failed to find the context");
            }
            int filter = 0;
            if (ev->readable) {
                filter |= io.READ;
            }
            if (ev->writable) {
                filter |= io.WRITE;
            }
            ctx->readyhandle = ev->handle;
            ctx->readyfilter = filter;
            ctx->waithandle = NULL;

            execute_context(&server, ctx);
            ev++;
        }
    }
    panic("unreachable");
}

lkcontext.LKContext *find_context(server_t *server, io.event_t *ev) {
    int filter = 0;
    if (ev->readable) {
        filter |= io.READ;
    }
    if (ev->writable) {
        filter |= io.WRITE;
    }
    llist.t *it = llist.next(server->contexts);
    while (it) {
        lkcontext.LKContext *ctx = it->data;
        if (ev->handle == ctx->waithandle && filter == ctx->waitfilter) {
            return ctx;
        }
        it = llist.next(it);
    }
    return NULL;
}

void execute_context(server_t *server, lkcontext.LKContext *ctx) {
    printf("executing context for %d\n", ctx->client_handle->fd);
    while (true) {
        printf("running line %d\n", ctx->current_line);
        int r = process(server, ctx, ctx->current_line);
        if (r == -1) {
            printf("routine finished\n");
            break;
        }
        if (r != ctx->current_line) {
            printf("routine moved from line %d to line %d\n", ctx->current_line, r);
        }
        ctx->current_line = r;
        if (ctx->waithandle) {
            if (ctx->waitfilter == io.READ) {
                printf("routine is waiting to read from %d)\n", ctx->waithandle->fd);
            }
            else if (ctx->waitfilter == io.WRITE) {
                printf("routine is waiting to write to %d\n", ctx->waithandle->fd);
            }
            else {
                printf("routine is waiting for fd %d (%d)\n", ctx->waithandle->fd, ctx->waitfilter);
            }
            break;
        }
    }
}

bool ready(lkcontext.LKContext *ctx, io.handle_t *h, int filter) {
    if (filter == io.READ) {
        printf("checking if %d is ready for reading\n", h->fd);
    }
    else if (filter == io.WRITE) {
        printf("checking if %d is ready for writing\n", h->fd);
    } else {
        printf("checking if %d is ready for %d\n", h->fd, filter);
    }
    if (ctx->readyhandle == h && ctx->readyfilter & filter) {
        printf("- ready\n");
        return true;
    }
    printf("- not ready, waiting\n");
    ctx->waithandle = h;
    ctx->waitfilter = filter;
    return false;
}

int process(server_t *server, lkcontext.LKContext *ctx, int line) {
    switch (line) {
    case 0:
        printf("reading a request\n");
        return 1;
    case 1:
        if (!ready(ctx, ctx->client_handle, io.READ)) {
            return 1;
        }
        if (!io.read(ctx->client_handle, ctx->inbuf)) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            return -1;
        }
        printf("read from %d, have %zu\n", ctx->client_handle->fd, io.bufsize(ctx->inbuf));
        if (!parse_request(ctx)) {
            return 1;
        }
        printf("parsed request\n");
        // Get the hostname from the request.
        http.request_t *req = &ctx->req->req;
        char *hostname = NULL;
        http.header_t *host = http.get_header(req, "Host");
        if (host) {
            hostname = host->value;
        }
        
        lkhostconfig.LKHostConfig *hc = lkconfig.lk_config_find_hostconfig(server->cfg, hostname);
        if (hc == NULL) {
            // process_error_response(server, ctx, 404, "LittleKitten webserver: hostconfig not found.");
            panic("todo");
            // return true;
        }
        printf("resolving %s\n", req->path);
        char *filepath = srvfiles.resolve_path(hc->homedir, req->path);
        if (!filepath) {
            printf("file \"%s\" not found, serving 404\n", req->path);
            srvstd.write_404(req, ctx);
            return 3;
        }

        printf("resolved %s as %s\n", req->path, filepath);
        const char *ext = fileutil.fileext(filepath);
        const char *content_type = mime.lookup(ext);
        if (content_type == NULL) {
            printf("didn't get MIME type for \"%s\", using text/plain\n", ext);
            content_type = "text/plain";
        }
        size_t filesize = 0;
        if (!fs.filesize(filepath, &filesize)) {
            panic("failed to get file size");
        }
        printf("writing headers\n");
        if (!io.pushf(ctx->outbuf,
            "%s 200 OK\n"
            "Content-Length: %ld\n"
            "Content-Type: %s\n\n\n",
            req->version,
            filesize,
            content_type
        )) {
            panic("failed to write response headers");
        }
    
        ctx->filehandle = io.open(filepath, "rb");
        if (!ctx->filehandle) {
            panic("oops");
        }
        printf("resolved file request to fd %d\n", ctx->filehandle->fd);
        return 2;
    case 2:
        if (!ready(ctx, ctx->filehandle, io.READ)) {
            return 2;
        }
        if (!io.read(ctx->filehandle, ctx->outbuf)) {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            return -1;
        }
        size_t n = io.bufsize(ctx->outbuf);
        printf("read %zu from the file\n", n);
        if (n == 0) {
            printf("closing file, requested finished\n");
            printf("output has %zu\n", io.bufsize(ctx->outbuf));
            io.close(ctx->filehandle);
            ctx->filehandle = NULL;
            return 0;
        }
        return 3;
    case 3:
        if (!ready(ctx, ctx->client_handle, io.WRITE)) {
            return 3;
        }
        if (!io.write(ctx->client_handle, ctx->outbuf)) {
            fprintf(stderr, "write failed: %s\n", strerror(errno));
            if (ctx->filehandle) {
                io.close(ctx->filehandle);
            }
            return -1;
        }
        return 2;
    }
    panic("unhandled state: %d", line);
}

bool parse_request(lkcontext.LKContext *ctx) {
    // See if the input buffer has a finished request header yet.
    char *split = strstr(ctx->inbuf->data, "\r\n\r\n");
    if (!split || (size_t)(split - ctx->inbuf->data) > ctx->inbuf->size) {
        return false;
    }
    split += 4;

    // Extract the headers and shift the buffer.
    char head[4096] = {0};
    size_t headlen = split - ctx->inbuf->data;
    memcpy(head, ctx->inbuf->data, headlen);
    io.shift(ctx->inbuf, headlen);
    // fprintf(stderr, "got a request:\n=============\n%s\n=============\n", head);

    // Parse the request.
    if (!http.parse_request(&ctx->req->req, head)) {
        panic("failed to parse the request");
    }

    // "goto" resolve.
    ctx->type = lkcontext.CTX_RESOLVE_REQ;
    return true;
}

// bool resolve_request(server_t *server, lkcontext.LKContext *ctx) {
//     // Get the hostname from the request.
//     http.request_t *req = &ctx->req->req;
//     char *hostname = NULL;
//     http.header_t *host = http.get_header(req, "Host");
//     if (host) {
//         hostname = host->value;
//     }

//     lkhostconfig.LKHostConfig *hc = lkconfig.lk_config_find_hostconfig(server->cfg, hostname);
//     if (hc == NULL) {
//         process_error_response(server, ctx, 404, "LittleKitten webserver: hostconfig not found.");
//         panic("todo");
//         return true;
//     }

//     // Forward request to proxyhost if proxyhost specified.
//     if (strlen(hc->proxyhost) > 0) {
//         panic("todo");
//         serve_proxy(server, ctx, hc->proxyhost);
//         return true;
//     }

//     if (strlen(hc->homedir) == 0) {
//         panic("todo");
//         process_error_response(server, ctx, 404, "LittleKitten webserver: hostconfig homedir not specified.");
//         return true;
//     }

//     // Replace path with any matching alias.
//     char *match = lkstringtable.lk_stringtable_get(hc->aliases, req->path);
//     if (match != NULL) {
//         panic("todo");
//         if (strlen(match) + 1 > sizeof(req->path)) {
//             abort();
//         }
//         strcpy(req->path, match);
//     }

//     // Run cgi script if uri falls under cgidir
//     printf("cgidir = %s, path = %s\n", hc->cgidir, req->path);
//     if (strlen(hc->cgidir) > 0 && strings.starts_with(req->path, hc->cgidir)) {
//         return srvcgi.resolve(ctx, hc);
//     }

//     // Fall back to static files.
//     return srvfiles.resolve(req, ctx, hc);
// }

// void process_writable_client(server_t *server, lkcontext.LKContext *ctx) {
//     if (ctx->type == lkcontext.CTX_WRITE_DATA) {
//         assert(ctx->resp != NULL);
//         assert(ctx->resp->head != NULL);
//         write_response(server, ctx);
//     } else if (ctx->type == lkcontext.CTX_WRITE_CGI_INPUT) {
//         write_cgi_input(server, ctx);
//     } else if (ctx->type == lkcontext.CTX_PROXY_WRITE_REQ) {
//         assert(ctx->req != NULL);
//         assert(ctx->req->head != NULL);
//         write_proxy_request(server, ctx);
//     } else {
//         printf("write_client: unknown ctx type %d\n", ctx->type);
//     }
// }


// // Read cgi output to cgi_outputbuf.
// void read_cgi_output(server_t *server, lkcontext.LKContext *ctx) {
//     int fd = 0;
//     int z = lknet.lk_read_all_file(fd, NULL);
//     if (z == lknet.Z_BLOCK) {
//         return;
//     }
//     if (z == lknet.Z_ERR) {
//         lk_print_err("lk_read_all_file()");
//         z = terminate_fd(ctx->cgifd, FD_READ, server);
//         if (z == 0) {
//             ctx->cgifd = 0;
//         }
//         process_error_response(server, ctx, 500, "Error processing CGI output.");
//         return;
//     }

//     // EOF - finished reading cgi output.
//     assert(z == 0);

//     // Remove cgi output from read list.
//     z = terminate_fd(ctx->cgifd, FD_READ, server);
//     if (z == 0) {
//         ctx->cgifd = 0;
//     }

//     lkhttpcgiparser.parse_cgi_output(ctx->cgi_outputbuf, ctx->resp);
//     process_response(server, ctx);
// }

// void process_response(server_t *server, lkcontext.LKContext *ctx) {
//     (void) server;
//     http.request_t *req = &ctx->req->req;
//     request.LKHttpResponse *resp = ctx->resp;

//     lk_httpresponse_finalize(resp);

//     // Clear response body on HEAD request.
//     if (!strcmp(req->method, "HEAD")) {
//         lkbuffer.lk_buffer_clear(resp->body);
//     }

//     char time_str[TIME_STRING_SIZE];
//     utils.get_localtime_string(time_str, sizeof(time_str));
//     printf("%s [%s] \"%s %s %s\" %d\n",
//         ctx->client_ipaddr->s, time_str,
//         req->method, req->uri, resp->version->s,
//         resp->status);
//     if (resp->status >= 500 && resp->status < 600 && resp->statustext->s_len > 0) {
//         printf("%s [%s] %d - %s\n",
//             ctx->client_ipaddr->s, time_str,
//             resp->status, resp->statustext->s);
//     }

//     // ctx->selectfd = ctx->clientfd;
//     ctx->type = lkcontext.CTX_WRITE_DATA;
//     // FD_SET_WRITE(ctx->selectfd, server);
//     lkreflist.lk_reflist_clear(ctx->buflist);
//     lkreflist.lk_reflist_append(ctx->buflist, resp->head);
//     lkreflist.lk_reflist_append(ctx->buflist, resp->body);
//     return;
// }

// Finalize the http response by setting head buffer.
// Writes the status line, headers and CRLF blank string to head buffer.
// void lk_httpresponse_finalize(request.LKHttpResponse *resp) {
//     lkbuffer.lk_buffer_clear(resp->head);

//     // Default to 200 OK if no status set.
//     if (resp->status == 0) {
//         resp->status = 200;
//         lkstring.lk_string_assign(resp->statustext, "OK");
//     }
//     // Default to HTTP version.
//     if (lkstring.lk_string_sz_equal(resp->version, "")) {
//         lkstring.lk_string_assign(resp->version, "HTTP/1.0");
//     }
//     lkbuffer.lk_buffer_append_sprintf(resp->head, "%s %d %s\n", resp->version->s, resp->status, resp->statustext->s);
//     lkbuffer.lk_buffer_append_sprintf(resp->head, "Content-Length: %ld\n", resp->body->bytes_len);
//     for (size_t i=0; i < resp->headers->items_len; i++) {
//         lkbuffer.lk_buffer_append_sprintf(resp->head, "%s: %s\n", resp->headers->items[i].k->s, resp->headers->items[i].v->s);
//     }
//     lkbuffer.lk_buffer_append(resp->head, "\r\n", 2);
// }

// void process_error_response(server_t *server, lkcontext.LKContext *ctx, int status, char *msg) {
//     request.LKHttpResponse *resp = ctx->resp;
//     resp->status = status;
//     lkstring.lk_string_assign(resp->statustext, msg);
//     lknet.lk_httpresponse_add_header(resp, "Content-Type", "text/plain");
//     lkbuffer.lk_buffer_append_sz(resp->body, msg);

//     process_response(server, ctx);
// }

// void write_response(server_t *server, lkcontext.LKContext *ctx) {
//     int fd = 0;
//     int z = lk_buflist_write_all(fd, FD_SOCK, ctx->buflist);
//     if (z == lknet.Z_BLOCK) {
//         return;
//     }
//     if (z == lknet.Z_ERR) {
//         lk_print_err("write_response lk_buflist_write_all()");
//         terminate_client_session(server, ctx);
//         return;
//     }
//     if (z == lknet.Z_EOF) {
//         // Completed sending http response.
//         terminate_client_session(server, ctx);
//     }
// }

// void serve_proxy(server_t *server, lkcontext.LKContext *ctx, char *targethost) {
//     net.net_t *proxyconn = net.net_open("tcp", targethost);
//     if (!proxyconn) {
//         lk_print_err("lk_open_connect_socket()");
//         process_error_response(server, ctx, 500, "Error opening proxy socket.");
//         return;
//     }

//     // hack
//     int proxyfd = proxyconn->fd;

//     lk_httprequest_finalize(ctx->req);
//     ctx->proxyfd = proxyfd;
//     // ctx->selectfd = proxyfd;
//     ctx->type = lkcontext.CTX_PROXY_WRITE_REQ;
//     // FD_SET_WRITE(proxyfd, server);
//     lkreflist.lk_reflist_clear(ctx->buflist);
//     lkreflist.lk_reflist_append(ctx->buflist, ctx->req->head);
//     lkreflist.lk_reflist_append(ctx->buflist, ctx->req->body);
// }

// void lk_httprequest_finalize(request.LKHttpRequest *wreq) {
//     http.request_t *req = &wreq->req;
//     lkbuffer.lk_buffer_clear(wreq->head);

//     // Default to HTTP version.
//     if (!strcmp(req->version, "")) {
//         strcpy(req->version, "HTTP/1.0");
//     }
//     lkbuffer.lk_buffer_append_sprintf(wreq->head, "%s %s %s\n", req->method, req->uri, req->version);
//     if (wreq->body->bytes_len > 0) {
//         lkbuffer.lk_buffer_append_sprintf(wreq->head, "Content-Length: %ld\n", wreq->body->bytes_len);
//     }
//     for (size_t i = 0; i < req->nheaders; i++) {
//         http.header_t *h = &req->headers[i];
//         lkbuffer.lk_buffer_append_sprintf(wreq->head, "%s: %s\n", h->name, h->value);
//     }
//     lkbuffer.lk_buffer_append(wreq->head, "\r\n", 2);
// }

// void write_proxy_request(server_t *server, lkcontext.LKContext *ctx) {
//     int fd = 0;
//     int z = lk_buflist_write_all(fd, FD_SOCK, ctx->buflist);
//     if (z == lknet.Z_BLOCK) {
//         return;
//     }
//     if (z == lknet.Z_ERR) {
//         lk_print_err("write_proxy_request lk_buflist_write_all");
//         z = terminate_sock(ctx->proxyfd, FD_WRITE, server);
//         if (z == 0) {
//             ctx->proxyfd = 0;
//         }
//         process_error_response(server, ctx, 500, "Error forwarding request to proxy.");
//         return;
//     }
//     if (z == lknet.Z_EOF) {
//         // Completed sending http request.
//         // FD_CLR_WRITE(ctx->selectfd, server);
//         // shutdown(ctx->selectfd, SHUT_WR);

//         // Pipe proxy response from ctx->proxyfd to ctx->clientfd
//         ctx->type = lkcontext.CTX_PROXY_PIPE_RESP;
//         ctx->proxy_respbuf = lkbuffer.lk_buffer_new(0);
//         // FD_SET_READ(ctx->selectfd, server);
//     }
// }

// // Similar to lknet.lk_write_all(), but sending buflist buf's sequentially.
// int lk_buflist_write_all(int fd, int fd_type, lkreflist.LKRefList *buflist) {
//     if (buflist->items_cur >= buflist->items_len) {
//         return lknet.Z_EOF;
//     }

//     lkbuffer.LKBuffer *buf = buflist->items[buflist->items_cur];
//     int z = lknet.lk_write_all(fd, fd_type, buf);
//     if (z == lknet.Z_EOF) {
//         buflist->items_cur++;
//         z = lknet.Z_OPEN;
//     }
//     return z;
// }

// void pipe_proxy_response(server_t *server, lkcontext.LKContext *ctx) {
//     int z = lk_pipe_all(ctx->proxyfd, 0, FD_SOCK, ctx->proxy_respbuf);
//     if (z == lknet.Z_OPEN || z == lknet.Z_BLOCK) {
//         return;
//     }
//     if (z == lknet.Z_ERR) {
//         lk_print_err("pipe_proxy_response lk_pipe_all()");
//         z = terminate_sock(ctx->proxyfd, FD_READ, server);
//         if (z == 0) {
//             ctx->proxyfd = 0;
//         }
//         process_error_response(server, ctx, 500, "Error reading/writing proxy response.");
//         return;
//     }

//     // EOF - finished reading/writing proxy response.
//     assert(z == lknet.Z_EOF);

//     // Remove proxy from read list.
//     z = terminate_sock(ctx->proxyfd, FD_READ, server);
//     if (z == 0) {
//         ctx->proxyfd = 0;
//     }

//     http.request_t *req = &ctx->req->req;
//     char time_str[TIME_STRING_SIZE];
//     utils.get_localtime_string(time_str, sizeof(time_str));
//     printf("%s [%s] \"%s %s\" --> proxyhost\n",
//         ctx->client_ipaddr->s, time_str, req->method, req->uri);

//     // Completed sending proxy response.
//     terminate_client_session(server, ctx);
// }

enum {
    FD_READ,
    FD_WRITE,
    FD_READWRITE
}; // fd_action

// // Clear fd from select()'s, shutdown, and OS.close.
// int terminate_fd(int fd, int fd_action, server_t *server) {
//     (void) server;
//     int z;
//     if (fd_action == FD_READ || fd_action == FD_READWRITE) {
//         // FD_CLR_READ(fd, server);
//     }
//     if (fd_action == FD_WRITE || fd_action == FD_READWRITE) {
//         // FD_CLR_WRITE(fd, server);
//     }
//     z = OS.close(fd);
//     if (z == -1) {
//         lk_print_err("OS.close()");
//     }
//     return z;
// }

// int terminate_sock(int fd, int fd_action, server_t *server) {
//     (void) server;
//     if (fd_action == FD_READ || fd_action == FD_READWRITE) {
//         // FD_CLR_READ(fd, server);
//     }
//     if (fd_action == FD_WRITE || fd_action == FD_READWRITE) {
//         // FD_CLR_WRITE(fd, server);
//     }
//     int z = OS.close(fd);
//     if (z == -1) {
//         lk_print_err("OS.close()");
//     }
//     return z;
// }

// Disconnect from client.
// void terminate_client_session(server_t *server, lkcontext.LKContext *ctx) {
//     (void) server;
//     (void) ctx;
//     // if (ctx->clientfd) {
//     //     terminate_sock(ctx->clientfd, FD_READWRITE, server);
//     // }
//     // if (ctx->cgifd) {
//     //     terminate_fd(ctx->cgifd, FD_READWRITE, server);
//     // }
//     // if (ctx->proxyfd) {
//     //     terminate_sock(ctx->proxyfd, FD_READWRITE, server);
//     // }
//     // Remove from linked list and free ctx.
//     // lkcontext.remove_client_context(&server->ctxhead, 0);
// }


// Pipe all available nonblocking readfd bytes into writefd.
// Uses buf as buffer for queued up bytes waiting to be written.
// Returns one of the following:
//    0 (lknet.Z_EOF) for read/write complete.
//    1 (lknet.Z_OPEN) for writefd socket OS.open
//   -1 (lknet.Z_ERR) for read/write error.
//   -2 (lknet.Z_BLOCK) for blocked readfd/writefd socket
// int lk_pipe_all(int readfd, int writefd, int fd_type, lkbuffer.LKBuffer *buf) {
//     int readz, writez;

//     readz = lknet.lk_read_all(readfd, fd_type, buf);
//     if (readz == lknet.Z_ERR) {
//         return readz;
//     }
//     assert(readz == lknet.Z_EOF || readz == lknet.Z_BLOCK);

//     writez = lknet.lk_write_all(writefd, fd_type, buf);
//     if (writez == lknet.Z_ERR) {
//         return writez;
//     }

//     // Pipe complete if no more data to read and all bytes sent.
//     if (readz == lknet.Z_EOF && writez == lknet.Z_EOF) {
//         return lknet.Z_EOF;
//     }

//     if (readz == lknet.Z_BLOCK) {
//         return lknet.Z_BLOCK;
//     }
//     if (writez == lknet.Z_EOF) {
//         writez = lknet.Z_OPEN;
//     }
//     return writez;
// }




// void lk_print_err(char *s) {
//     fprintf(stderr, "%s: %s\n", s, strerror(errno));
// }

// bool process_event(server_t *s, io.event_t *ev) {
//     llist.t *it = llist.next(s->contexts);
//     while (it) {
//         lkcontext.LKContext *ctx = it->data;
//         if (ev->handle == ctx->waithandle) {
//             ctx->readyhandle = ev->handle;
//             int filter = 0;
//             if (ev->readable) {
//                 filter |= io.READ;
//             }
//             if (ev->writable) {
//                 filter |= io.WRITE;
//             }
//             ctx->readyfilter = filter;
//             // // Have space and can read
//             // if (ev->readable && io.bufspace(ctx->inbuf) > 0) {
//             //     if (!io.read(ev->handle, ctx->inbuf)) {
//             //         fprintf(stderr, "read failed: %s\n", strerror(errno));
//             //         return false;
//             //     }
//             // }
//             // // Have data to write and can write
//             // if (ev->writable && io.bufsize(ctx->outbuf) > 0) {
//             //     if (!io.write(ev->handle, ctx->outbuf)) {
//             //         fprintf(stderr, "write failed: %s\n", strerror(errno));
//             //         return false;
//             //     }
//             // }
//             return process_ctx(s, ctx);
//         }
//         // if (ev->handle == ctx->data_handle) {
//         //     if (ev->readable && io.bufspace(ctx->outbuf)) {
//         //         if (!io.read(ev->handle, ctx->outbuf)) {
//         //             fprintf(stderr, "read failed: %s\n", strerror(errno));
//         //             return false;
//         //         }
//         //     }
//         // }
//         return process_ctx(s, ctx);
//         it = llist.next(it);
//     }
//     fprintf(stderr, "failed to find ctx for the event\n");
//     return false;
// }