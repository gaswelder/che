#import lkstring.c

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
