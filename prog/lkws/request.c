#import lkstring.c
#import lkstringtable.c
#import lkbuffer.c

pub typedef {
    char method[10]; // GET
    char uri[1024]; // "/path/to/index.html?p=1&start=5"
    char path[1024];         // "/path/to/index.html"
    char filename[1024];     // "index.html"
    char querystring[1024];  // "p=1&start=5"
    char version[20];      // HTTP/1.0

    lkstringtable.LKStringTable *headers;
    lkbuffer.LKBuffer *head;
    lkbuffer.LKBuffer *body;
} LKHttpRequest;

pub LKHttpRequest *lk_httprequest_new() {
    LKHttpRequest *req = calloc(1, sizeof(LKHttpRequest));
    req->headers = lkstringtable.lk_stringtable_new();
    req->head = lkbuffer.lk_buffer_new(0);
    req->body = lkbuffer.lk_buffer_new(0);
    return req;
}

pub void lk_httprequest_free(LKHttpRequest *req) {
    lkstringtable.lk_stringtable_free(req->headers);
    lkbuffer.lk_buffer_free(req->head);
    lkbuffer.lk_buffer_free(req->body);

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
