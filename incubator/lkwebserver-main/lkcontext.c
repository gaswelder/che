#import lkalloc.c
#import lkbuffer.c
#import lkhttprequestparser.c
#import lkreflist.c
#import lkstring.c
#import request.c
#import socketreader.c

pub enum {
    CTX_READ_REQ,
    CTX_READ_CGI_OUTPUT,
    CTX_WRITE_CGI_INPUT,
    CTX_WRITE_RESP,
    CTX_PROXY_WRITE_REQ,
    CTX_PROXY_PIPE_RESP
}; // LKContextType;

pub typedef {
    int selectfd;
    int clientfd;
    int type;
    LKContext *next;         // link to next ctx

    // Used by CTX_READ_REQ:
    lkstring.LKString *client_ipaddr;          // client ip address string
    int client_port;       // client port number
    lkstring.LKString *req_line;               // current lklib line
    lkbuffer.LKBuffer *req_buf;                // current lklib bytes buffer
    socketreader.LKSocketReader *sr;               // input buffer for reading lines
    lkhttprequestparser.LKHttpRequestParser *reqparser;   // parser for httprequest
    request.LKHttpRequest *req;               // http lklib in process

    // Used by CTX_WRITE_REQ:
    request.LKHttpResponse *resp;             // http response to be sent
    lkreflist.LKRefList *buflist;               // Buffer list of things to send/recv

    // Used by CTX_READ_CGI:
    int cgifd;
    lkbuffer.LKBuffer *cgi_outputbuf;          // receive cgi stdout bytes here
    lkbuffer.LKBuffer *cgi_inputbuf;           // input bytes to pass to cgi stdin

    // Used by CTX_PROXY_WRITE_REQ:
    int proxyfd;
    lkbuffer.LKBuffer *proxy_respbuf;
} LKContext;

/*** LKContext functions ***/
pub LKContext *lk_context_new() {
    LKContext *ctx = lkalloc.lk_malloc(sizeof(LKContext), "lk_context_new");
    ctx->selectfd = 0;
    ctx->clientfd = 0;
    ctx->type = 0;
    ctx->next = NULL;

    ctx->client_ipaddr = NULL;
    ctx->client_port = 0;

    ctx->req_line = NULL;
    ctx->req_buf = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->req = NULL;
    ctx->resp = NULL;
    ctx->buflist = NULL;

    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;

    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;

    return ctx;
}

pub LKContext *create_initial_context(int fd) {
    LKContext *ctx = lkalloc.lk_malloc(sizeof(LKContext), "create_initial_context");
    ctx->selectfd = fd;
    ctx->clientfd = fd;
    ctx->type = CTX_READ_REQ;
    ctx->next = NULL;

    ctx->req_line = lkstring.lk_string_new("");
    ctx->req_buf = lkbuffer.lk_buffer_new(0);
    ctx->sr = socketreader.lk_socketreader_new(fd, 0);
    ctx->reqparser = lkhttprequestparser.lk_httprequestparser_new();
    ctx->req = request.lk_httprequest_new();
    ctx->resp = request.lk_httpresponse_new();
    ctx->buflist = lkreflist.lk_reflist_new();

    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;

    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;

    return ctx;
}

pub void lk_context_free(LKContext *ctx) {
    if (ctx->client_ipaddr) {
        lkstring.lk_string_free(ctx->client_ipaddr);
    }
    if (ctx->req_line) {
        lkstring.lk_string_free(ctx->req_line);
    }
    if (ctx->req_buf) {
        lkbuffer.lk_buffer_free(ctx->req_buf);
    }
    if (ctx->sr) {
        socketreader.lk_socketreader_free(ctx->sr);
    }
    if (ctx->reqparser) {
        lkhttprequestparser.lk_httprequestparser_free(ctx->reqparser);
    }
    if (ctx->req) {
        request.lk_httprequest_free(ctx->req);
    }
    if (ctx->resp) {
        request.lk_httpresponse_free(ctx->resp);
    }
    if (ctx->buflist) {
        lkreflist.lk_reflist_free(ctx->buflist);
    }
    if (ctx->cgi_outputbuf) {
        lkbuffer.lk_buffer_free(ctx->cgi_outputbuf);
    }
    if (ctx->cgi_inputbuf) {
        lkbuffer.lk_buffer_free(ctx->cgi_inputbuf);
    }
    if (ctx->proxy_respbuf) {
        lkbuffer.lk_buffer_free(ctx->proxy_respbuf);
    }

    ctx->selectfd = 0;
    ctx->clientfd = 0;
    ctx->next = NULL;
    ctx->client_ipaddr = NULL;
    ctx->req_line = NULL;
    ctx->req_buf = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->req = NULL;
    ctx->resp = NULL;
    ctx->buflist = NULL;
    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;
    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;
    lkalloc.lk_free(ctx);
}

// Add new client ctx to end of ctx linked list.
// Skip if ctx clientfd already in list.
pub void add_new_client_context(LKContext **pphead, LKContext *ctx) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        // first client
        *pphead = ctx;
    } else {
        // add client to end of clients list
        LKContext *p = *pphead;
        while (p->next != NULL) {
            // ctx fd already exists
            if (p->clientfd == ctx->clientfd) {
                return;
            }
            p = p->next;
        }
        p->next = ctx;
    }
}

// Add ctx to end of ctx linked list, allowing duplicate clientfds.
pub void add_context(LKContext **pphead, LKContext *ctx) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        // first client
        *pphead = ctx;
    } else {
        // add to end of clients list
        LKContext *p = *pphead;
        while (p->next != NULL) {
            p = p->next;
        }
        p->next = ctx;
    }
}


// Delete first ctx having clientfd from linked list.
// Returns 1 if context was deleted, 0 if no deletion made.
pub int remove_client_context(LKContext **pphead, int clientfd) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        return 0;
    }
    // remove head ctx
    if ((*pphead)->clientfd == clientfd) {
        LKContext *tmp = *pphead;
        *pphead = (*pphead)->next;
        lk_context_free(tmp);
        return 1;
    }

    LKContext *p = *pphead;
    LKContext *prev = NULL;
    while (p != NULL) {
        if (p->clientfd == clientfd) {
            assert(prev != NULL);
            prev->next = p->next;
            lk_context_free(p);
            return 1;
        }

        prev = p;
        p = p->next;
    }

    return 0;
}

// Return ctx matching selectfd.
pub LKContext *match_select_ctx(LKContext *phead, int selectfd) {
    LKContext *ctx = phead;
    while (ctx != NULL) {
        if (ctx->selectfd == selectfd) {
            break;
        }
        ctx = ctx->next;
    }
    return ctx;
}

// Delete first ctx having selectfd from linked list.
// Returns 1 if context was deleted, 0 if no deletion made.
pub int remove_selectfd_context(LKContext **pphead, int selectfd) {
    assert(pphead != NULL);

    if (*pphead == NULL) {
        return 0;
    }
    // remove head ctx
    if ((*pphead)->selectfd == selectfd) {
        LKContext *tmp = *pphead;
        *pphead = (*pphead)->next;
        lk_context_free(tmp);
        return 1;
    }

    LKContext *p = *pphead;
    LKContext *prev = NULL;
    while (p != NULL) {
        if (p->selectfd == selectfd) {
            assert(prev != NULL);
            prev->next = p->next;
            lk_context_free(p);
            return 1;
        }

        prev = p;
        p = p->next;
    }

    return 0;
}
