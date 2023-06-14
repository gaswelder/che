#import lkstring.c
#import lkstringtable.c
#import lkbuffer.c

pub typedef {
    lkstring.LKString *method;       // GET
    lkstring.LKString *uri;          // "/path/to/index.html?p=1&start=5"
    lkstring.LKString *path;         // "/path/to/index.html"
    lkstring.LKString *filename;     // "index.html"
    lkstring.LKString *querystring;  // "p=1&start=5"
    lkstring.LKString *version;      // HTTP/1.0
    lkstringtable.LKStringTable *headers;
    lkbuffer.LKBuffer *head;
    lkbuffer.LKBuffer *body;
} LKHttpRequest;

pub LKHttpRequest *lk_httprequest_new() {
    LKHttpRequest *req = calloc(1, sizeof(LKHttpRequest));
    req->method = lkstring.lk_string_new("");
    req->uri = lkstring.lk_string_new("");
    req->path = lkstring.lk_string_new("");
    req->filename = lkstring.lk_string_new("");
    req->querystring = lkstring.lk_string_new("");
    req->version = lkstring.lk_string_new("");
    req->headers = lkstringtable.lk_stringtable_new();
    req->head = lkbuffer.lk_buffer_new(0);
    req->body = lkbuffer.lk_buffer_new(0);
    return req;
}

pub void lk_httprequest_free(LKHttpRequest *req) {
    lkstring.lk_string_free(req->method);
    lkstring.lk_string_free(req->uri);
    lkstring.lk_string_free(req->path);
    lkstring.lk_string_free(req->filename);
    lkstring.lk_string_free(req->querystring);
    lkstring.lk_string_free(req->version);
    lkstringtable.lk_stringtable_free(req->headers);
    lkbuffer.lk_buffer_free(req->head);
    lkbuffer.lk_buffer_free(req->body);

    req->method = NULL;
    req->uri = NULL;
    req->path = NULL;
    req->filename = NULL;
    req->querystring = NULL;
    req->version = NULL;
    req->headers = NULL;
    req->head = NULL;
    req->body = NULL;
    free(req);
}

pub void lk_httprequest_add_header(LKHttpRequest *req, char *k, char *v) {
    lkstringtable.lk_stringtable_set(req->headers, k, v);
}


pub typedef {
    int status;             // 404
    lkstring.LKString *statustext;    // File not found
    lkstring.LKString *version;       // HTTP/1.0
    lkstringtable.LKStringTable *headers;
    lkbuffer.LKBuffer *head;
    lkbuffer.LKBuffer *body;
} LKHttpResponse;

pub LKHttpResponse *lk_httpresponse_new() {
    LKHttpResponse *resp = calloc(1, sizeof(LKHttpResponse));
    resp->status = 0;
    resp->statustext = lkstring.lk_string_new("");
    resp->version = lkstring.lk_string_new("");
    resp->headers = lkstringtable.lk_stringtable_new();
    resp->head = lkbuffer.lk_buffer_new(0);
    resp->body = lkbuffer.lk_buffer_new(0);
    return resp;
}

pub void lk_httpresponse_free(LKHttpResponse *resp) {
    lkstring.lk_string_free(resp->statustext);
    lkstring.lk_string_free(resp->version);
    lkstringtable.lk_stringtable_free(resp->headers);
    lkbuffer.lk_buffer_free(resp->head);
    lkbuffer.lk_buffer_free(resp->body);

    resp->statustext = NULL;
    resp->version = NULL;
    resp->headers = NULL;
    resp->head = NULL;
    resp->body = NULL;
    free(resp);
}
