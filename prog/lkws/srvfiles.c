#import strings
#import fs
#import http
#import mime
#import os/io
#import fileutil

#import lkcontext.c
#import lkhostconfig.c

const char *default_files[] = {
    "index.html",
    "index.htm",
    "default.html",
    "default.htm"
};

const char *NOT_FOUND_MESSAGE = "File not found on the server.";

pub bool resolve(http.request_t *req, lkcontext.LKContext *ctx, lkhostconfig.LKHostConfig *hc) {
    if (!strcmp(req->method, "GET")) {
        printf("processing GET\n");
        char *filepath = resolve_path(hc->homedir->s, req->path);
        if (!filepath) {
            printf("file not found, serving 404\n");
            bool ok = io.pushf(ctx->outbuf,
                "%s 404 Not Found\n"
                "Content-Length: %ld\n"
                "Content-Type: text/plain\n"
                "\n"
                "\n"
                "%s",
                req->version,
                strlen(NOT_FOUND_MESSAGE),
                NOT_FOUND_MESSAGE
            );
            if (!ok) {
                panic("failed to write to the output buffer");
            }
            ctx->type = lkcontext.CTX_WRITE_DATA;
            return true;
        }

        // Read the file
        size_t filesize = 0;
        char *data = fileutil.readfile(filepath, &filesize);
        if (!data) {
            panic("failed to read file '%s'", filepath);
        }
        const char *content_type = mime.lookup(fileutil.fileext(filepath));
        if (content_type == NULL) {
            content_type = "text/plain";
        }

        // Format the response.
        bool ok = io.pushf(ctx->outbuf,
            "%s 200 OK\n"
            "Content-Length: %ld\n"
            "Content-Type: %s\n"
            "\n",
            req->version,
            filesize,
            content_type
        );
        if (!ok) {
            panic("failed to write response headers");
        }

        if (!io.push(ctx->outbuf, data, filesize)) {
            panic("failed to write data to the buffer");
        }
        
        ctx->type = lkcontext.CTX_WRITE_DATA;
        free(filepath);
        return true;
    }

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

    panic("unsupported method: '%s'", req->method);
}

char *resolve_path(const char *homedir, *reqpath) {
    if (!strcmp(reqpath, "")) {
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
    sprintf(naive_path, "%s/%s", homedir, reqpath);
    printf("naive path = %s\n", naive_path);

    char realpath[4096] = {0};
    if (!fs.realpath(naive_path, realpath, sizeof(realpath))) {
        printf("couldn't resolve realpath\n");
        return NULL;
    }
    printf("real path = %s\n", realpath);

    if (!strings.starts_with(realpath, homedir)) {
        printf("resolved path doesn't start with homedir=%s\n", homedir);
        return NULL;
    }

    return strings.newstr("%s", realpath);
}
