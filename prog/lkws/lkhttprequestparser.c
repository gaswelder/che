#import strings

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
        parse_request_line(line, req);
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

// Parse initial request line in the format:
// GET /path/to/index.html HTTP/1.0
void parse_request_line(char *line, request.LKHttpRequest *req) {
    char *toks[3];
    int ntoksread = 0;

    char *saveptr;
    char *delim = " \t";
    char *linetmp = strings.newstr("%s", line);
    lknet.lk_chomp(linetmp);
    char *p = linetmp;
    while (ntoksread < 3) {
        toks[ntoksread] = OS.strtok_r(p, delim, &saveptr);
        if (toks[ntoksread] == NULL) {
            break;
        }
        ntoksread++;
        p = NULL; // for the next call to OS.strtok_r()
    }

    char *method = "";
    char *uri = "";
    char *version = "";

    if (ntoksread > 0) method = toks[0];
    if (ntoksread > 1) uri = toks[1];
    if (ntoksread > 2) version = toks[2];

    if (strlen(method) + 1 > sizeof(req->method)) {
        abort();
    }
    if (strlen(uri) + 1 > sizeof(req->uri)) {
        abort();
    }
    strcpy(req->method, method);
    strcpy(req->uri, uri);
    lkstring.lk_string_assign(req->version, version);

    lkstring.LKString *uriwrapper = lkstring.lk_string_new(uri);
    lkstring.LKString *pathwrapper = lkstring.lk_string_new(req->path);
    lkstring.LKString *filenamewrapper = lkstring.lk_string_new(req->filename);
    parse_uri(uriwrapper, pathwrapper, filenamewrapper, req->querystring);
    strcpy(req->uri, uriwrapper->s);
    strcpy(req->path, pathwrapper->s);
    strcpy(req->filename, filenamewrapper->s);
    lkstring.lk_string_free(uriwrapper);
    lkstring.lk_string_free(pathwrapper);
    lkstring.lk_string_free(filenamewrapper);

    free(linetmp);
}

// Parse uri into its components.
// Given: lks_uri   = "/path/blog/file1.html?a=1&b=2"
// lks_path         = "/path/blog/file1.html"
// lks_filename     = "file1.html"
// lks_qs           = "a=1&b=2"
void parse_uri(lkstring.LKString *lks_uri, lkstring.LKString *lks_path, lkstring.LKString *lks_filename, lkstring.LKString *lks_qs) { 
    // Get path and querystring
    // "/path/blog/file1.html?a=1&b=2" ==> "/path/blog/file1.html" and "a=1&b=2"
    lkstring.LKStringList *uri_ss = lkstring.lk_string_split(lks_uri, "?");
    lkstring.lk_string_assign(lks_path, uri_ss->items[0]->s);
    if (uri_ss->items_len > 1) {
        lkstring.lk_string_assign(lks_qs, uri_ss->items[1]->s);
    } else {
        lkstring.lk_string_assign(lks_qs, "");
    }

    // Remove any trailing slash from uri. "/path/blog/" ==> "/path/blog"
    lkstring.lk_string_chop_end(lks_path, "/");

    // Extract filename from path. "/path/blog/file1.html" ==> "file1.html"
    lkstring.LKStringList *path_ss = lkstring.lk_string_split(lks_path, "/");
    assert(path_ss->items_len > 0);
    lkstring.lk_string_assign(lks_filename, path_ss->items[path_ss->items_len-1]->s);

    lkstring.lk_stringlist_free(uri_ss);
    lkstring.lk_stringlist_free(path_ss);
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

