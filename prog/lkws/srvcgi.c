#import fileutil
#import os/misc
#import strings
#import http
#import fs

#import lkbuffer.c
#import lkcontext.c
#import lkhostconfig.c
#import lknet.c
#import lkreflist.c
#import lkstring.c
#import request.c
#import utils.c
#import srvstd.c

pub bool resolve(lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    printf("resolving CGI\n");
    request.LKHttpRequest *req = ctx->req;
    request.LKHttpResponse *resp = ctx->resp;
    char *path = ctx->req->req.path;

    char *cgifile = strings.newstr("%s/%s", hc->homedir_abspath, path);
    printf("cgifile = %s\n", cgifile);

    // Expand "/../", etc. into real_path.
    char real_path[PATH_MAX];
    bool pathok = fs.realpath(cgifile, real_path, sizeof(real_path));
    free(cgifile);
    printf("realpath = %s\n", real_path);

    // real_path should start with cgidir_abspath
    // real_path file should exist
    if (!pathok
        || !strings.starts_with(real_path, hc->cgidir_abspath)
        || !fileutil.file_exists(real_path)
    ) {
        printf("invalid path\n");
        srvstd.write_404(&req->req, ctx);
        ctx->type = lkcontext.CTX_WRITE_DATA;
        return true;
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
        process_response(ctx);
        return true;
    }

    OS.close(fd_in);

    // Read cgi output in select()
    // ctx->selectfd = fd_out;
    ctx->cgifd = fd_out;
    ctx->type = lkcontext.CTX_READ_CGI_OUTPUT;
    ctx->cgi_outputbuf = lkbuffer.lk_buffer_new(0);

    // If req is POST with body, pass it to cgi process stdin.
    if (req->body->bytes_len > 0) {
        lkcontext.LKContext *ctx_in = lkcontext.lk_context_new();
        // llist.append(server->contexts, ctx_in);

        // ctx_in->selectfd = fd_in;
        ctx_in->cgifd = fd_in;
        // ctx_in->clientfd = ctx->clientfd;
        ctx_in->type = lkcontext.CTX_WRITE_CGI_INPUT;

        ctx_in->cgi_inputbuf = lkbuffer.lk_buffer_new(0);
        lkbuffer.lk_buffer_append(ctx_in->cgi_inputbuf, req->body->bytes, req->body->bytes_len);

        // FD_SET_WRITE(ctx_in->selectfd, server);
    }
    return true;
}

// Sets the cgi environment variables that vary for each http request.
void set_cgi_env2(lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    request.LKHttpRequest *wreq = ctx->req;
    http.request_t *req = &wreq->req;

    misc.setenv("DOCUMENT_ROOT", hc->homedir_abspath, 1);

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

    
    

    lkstring.LKString *lkscript_filename = lkstring.lk_string_new(hc->homedir_abspath);
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

// // Send cgi_inputbuf input bytes to cgi program stdin set in selectfd.
// void write_cgi_input(server_t *server, lkcontext.LKContext *ctx) {
//     assert(ctx->cgi_inputbuf != NULL);
//     int fd = ctx->data_handle->fd;
//     int z = lknet.lk_write_all_file(fd, ctx->cgi_inputbuf);
//     if (z == lknet.Z_BLOCK) {
//         return;
//     }
//     if (z == lknet.Z_ERR) {
//         lk_print_err("write_cgi_input lknet.lk_write_all_file()");
//         z = terminate_fd(ctx->cgifd, FD_WRITE, server);
//         if (z == 0) {
//             ctx->cgifd = 0;
//         }
//         // lkcontext.remove_selectfd_context(&server->ctxhead, fd);
//         return;
//     }
//     if (z == lknet.Z_EOF) {
//         // Completed writing input bytes.
//         // FD_CLR_WRITE(fd, server);
//         // shutdown(fd, SHUT_WR);
//         // lkcontext.remove_selectfd_context(&server->ctxhead, fd);
//     }
// }

void process_response(lkcontext.LKContext *ctx) {
    http.request_t *req = &ctx->req->req;
    request.LKHttpResponse *resp = ctx->resp;

    lk_httpresponse_finalize(resp);

    // Clear response body on HEAD request.
    if (!strcmp(req->method, "HEAD")) {
        lkbuffer.lk_buffer_clear(resp->body);
    }

    char time_str[1000];
    utils.get_localtime_string(time_str, sizeof(time_str));
    printf("%s [%s] \"%s %s %s\" %d\n", 
        ctx->client_ipaddr->s, time_str,
        req->method, req->uri, resp->version->s,
        resp->status);
    if (resp->status >= 500 && resp->status < 600 && resp->statustext->s_len > 0) {
        printf("%s [%s] %d - %s\n", 
            ctx->client_ipaddr->s, time_str,
            resp->status, resp->statustext->s);
    }

    // ctx->selectfd = ctx->clientfd;
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
