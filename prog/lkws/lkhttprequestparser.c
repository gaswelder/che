#import strings
#import http

#import lkbuffer.c
#import lknet.c
#import lkstring.c
#import request.c
#import utils.c

pub typedef {
    lkstring.LKString *partial_line;
    size_t nlinesread;
    int head_complete;              // flag indicating header lines complete
    int body_complete;              // flag indicating request body complete
    size_t content_length;    // value of Content-Length header
} LKHttpRequestParser;

pub LKHttpRequestParser *lk_httprequestparser_new() {
    LKHttpRequestParser *parser = calloc(1, sizeof(LKHttpRequestParser));
    parser->partial_line = lkstring.lk_string_new("");
    parser->nlinesread = 0;
    parser->content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;
    return parser;
}

pub void lk_httprequestparser_free(LKHttpRequestParser *parser) {
    lkstring.lk_string_free(parser->partial_line);
    parser->partial_line = NULL;
    free(parser);
}

// Clear any pending state.
pub void lk_httprequestparser_reset(LKHttpRequestParser *parser) {
    lkstring.lk_string_assign(parser->partial_line, "");
    parser->nlinesread = 0;
    parser->content_length = 0;
    parser->head_complete = 0;
    parser->body_complete = 0;
}

// Parse one line and cumulatively compile results into req.
// You can check the state of the parser through the following fields:
// parser->head_complete   Request Line and Headers complete
// parser->body_complete   httprequest is complete
pub void lk_httprequestparser_parse_line(LKHttpRequestParser *parser, lkstring.LKString *line, request.LKHttpRequest *req) {
    // If there's a previous partial line, combine it with current line.
    if (parser->partial_line->s_len > 0) {
        lkstring.lk_string_append(parser->partial_line, line->s);
        if (lkstring.lk_string_ends_with(parser->partial_line, "\n")) {
            parse_line(parser, parser->partial_line->s, req);
            lkstring.lk_string_assign(parser->partial_line, "");
        }
        return;
    }

    // If current line is only partial line (not newline terminated), remember it for
    // next read.
    if (!lkstring.lk_string_ends_with(line, "\n")) {
        lkstring.lk_string_assign(parser->partial_line, line->s);
        return;
    }

    parse_line(parser, line->s, req);
}

void parse_line(LKHttpRequestParser *parser, char *line, request.LKHttpRequest *req) {
    // First line: parse initial request line.
    if (parser->nlinesread == 0) {
        http.request_line_t r = {};
        if (!http.parse_request_line(line, &r)) {
            abort();
        }
        strcpy(req->method, r.method);
        strcpy(req->uri, r.uri);
        strcpy(req->path, r.path);
        strcpy(req->filename, r.filename);
        strcpy(req->querystring, r.query);
        strcpy(req->version, r.version);
        parser->nlinesread++;
        return;
    }

    parser->nlinesread++;

    // Header lines
    if (!parser->head_complete) {
        // Empty CRLF line ends the headers section
        if (utils.is_empty_line(line)) {
            parser->head_complete = 1;

            // No body to read (Content-Length: 0)
            if (parser->content_length == 0) {
                parser->body_complete = 1;
            }
            return;
        }
        parse_header_line(parser, line, req);
        return;
    }
}

// Parse header line in the format Ex. User-Agent: browser
void parse_header_line(LKHttpRequestParser *parser, char *line, request.LKHttpRequest *req) {
    char *saveptr;
    char *delim = ":";

    char *linetmp = strings.newstr("%s", line);
    lknet.lk_chomp(linetmp);
    char *k = OS.strtok_r(linetmp, delim, &saveptr);
    if (k == NULL) {
        free(linetmp);
        return;
    }
    char *v = OS.strtok_r(NULL, delim, &saveptr);
    if (v == NULL) {
        v = "";
    }

    // skip over leading whitespace
    while (*v == ' ' || *v == '\t') {
        v++;
    }
    request.lk_httprequest_add_header(req, k, v);

    if (!strings.casecmp(k, "Content-Length")) {
        int content_length = atoi(v);
        parser->content_length = content_length;
    }

    free(linetmp);
}


// Parse sequence of bytes into request body. Compile results into req.
// You can check the state of the parser through the following fields:
// parser->head_complete   Request Line and Headers complete
// parser->body_complete   httprequest is complete
pub void lk_httprequestparser_parse_bytes(LKHttpRequestParser *parser, lkbuffer.LKBuffer *buf, request.LKHttpRequest *req) {
    // Head should be parsed line by line. Call parse_line() instead.
    if (!parser->head_complete) {
        return;
    }
    if (parser->body_complete) {
        return;
    }

    lkbuffer.lk_buffer_append(req->body, buf->bytes, buf->bytes_len);
    if (req->body->bytes_len >= parser->content_length) {
        parser->body_complete = 1;
    }
}

