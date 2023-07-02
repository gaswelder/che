#import os/io
#import http

#import lkbuffer.c
#import lkhostconfig.c
#import lkhttprequestparser.c
#import lkreflist.c
#import lkstring.c
#import request.c
#import socketreader.c

/**
 * Possible context states.
 * Since this server is a single-process one, these states are effectively
 * opcodes for the main loop, telling what proceduce has to be called for
 * a particular context on each iteration.
 */
pub enum {
    CTX_READ_REQ, // read a request
    CTX_RESOLVE_REQ, // look at the request, decide what to do
    CTX_WRITE_DATA, // write data from the output buffer

    CTX_READ_CGI_OUTPUT,
    CTX_WRITE_CGI_INPUT,
    
    CTX_PROXY_WRITE_REQ,
    CTX_PROXY_PIPE_RESP
}; // LKContextType;

pub typedef {
    io.handle_t *client_handle; // Connected client.
    io.buf_t *inbuf, *outbuf; // Buffers for incoming and outgoing data.

    /*
     * Current parsed request and the corresponding host config.
     */
    http.request_t req;
    lkhostconfig.LKHostConfig *hc;

    int subroutine;

    // files serving routine
    io.handle_t *filehandle; // for serving a local file.



    // ...

    io.handle_t *data_handle;
    
    int type; // state, one of CTX_* constants
   

  
    

    // Used by CTX_READ_REQ:
    lkstring.LKString *client_ipaddr;          // client ip address string
    int client_port;       // client port number
    lkstring.LKString *req_line;               // current line
    lkbuffer.LKBuffer *req_buf;                // current bytes buffer
    socketreader.LKSocketReader *sr;               // input buffer for reading lines
    lkhttprequestparser.LKHttpRequestParser *reqparser;   // parser for httprequest
    

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
    LKContext *ctx = calloc(1, sizeof(LKContext));
    ctx->type = CTX_READ_REQ;
    ctx->inbuf = io.newbuf();
    ctx->outbuf = io.newbuf();

    ctx->client_ipaddr = NULL;
    ctx->client_port = 0;

    ctx->req_line = NULL;
    ctx->req_buf = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->resp = NULL;
    ctx->buflist = NULL;

    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;

    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;

    return ctx;
}

pub LKContext *create_initial_context(io.handle_t *h) {
    LKContext *ctx = calloc(1, sizeof(LKContext));
    ctx->type = CTX_READ_REQ;
    ctx->client_handle = h;
    ctx->inbuf = io.newbuf();
    ctx->outbuf = io.newbuf();

    ctx->req_line = lkstring.lk_string_new("");
    ctx->req_buf = lkbuffer.lk_buffer_new(0);
    ctx->reqparser = lkhttprequestparser.lk_httprequestparser_new();
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
    io.freebuf(ctx->inbuf);
    io.freebuf(ctx->outbuf);
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

    ctx->client_ipaddr = NULL;
    ctx->req_line = NULL;
    ctx->req_buf = NULL;
    ctx->sr = NULL;
    ctx->reqparser = NULL;
    ctx->resp = NULL;
    ctx->buflist = NULL;
    ctx->cgifd = 0;
    ctx->cgi_outputbuf = NULL;
    ctx->cgi_inputbuf = NULL;
    ctx->proxyfd = 0;
    ctx->proxy_respbuf = NULL;
    free(ctx);
}

