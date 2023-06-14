#import strings

#import lkalloc.c
#import lkbuffer.c
#import lklib.c
#import lknet.c
#import lkstringtable.c
#import request.c

#define LK_BUFSIZE_MEDIUM 1024

pub void parse_cgi_output(lkbuffer.LKBuffer *buf, request.LKHttpResponse *resp) {
    char cgiline[LK_BUFSIZE_MEDIUM];
    int has_crlf = 0;

    // Parse cgi_outputbuf line by line into http response.
    while (buf->bytes_cur < buf->bytes_len) {
        lkbuffer.lk_buffer_readline(buf, cgiline, sizeof(cgiline));
        lknet.lk_chomp(cgiline);

        // Empty CRLF line ends the headers section
        if (lklib.is_empty_line(cgiline)) {
            has_crlf = 1;
            break;
        }
        parse_cgi_header_line(cgiline, resp);
    }
    if (buf->bytes_cur < buf->bytes_len) {
        lkbuffer.lk_buffer_append(
            resp->body,
            buf->bytes + buf->bytes_cur,
            buf->bytes_len - buf->bytes_cur
        );
    }

    char *content_type = lkstringtable.lk_stringtable_get(resp->headers, "Content-Type");

    // If cgi error
    // copy cgi output as is to display the error messages.
    if (!has_crlf && content_type == NULL) {
        lkstringtable.lk_stringtable_set(resp->headers, "Content-Type", "text/plain");
        lkbuffer.lk_buffer_clear(resp->body);
        lkbuffer.lk_buffer_append(resp->body, buf->bytes, buf->bytes_len);
    }
}

// Parse header line in the format Ex. User-Agent: browser
void parse_cgi_header_line(char *line, request.LKHttpResponse *resp) {
    char *saveptr;
    char *delim = ":";

    char *linetmp = lkalloc.lk_strdup(line, "parse_cgi_header_line");
    lknet.lk_chomp(linetmp);
    char *k = OS.strtok_r(linetmp, delim, &saveptr);
    if (k == NULL) {
        lkalloc.lk_free(linetmp);
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
    lknet.lk_httpresponse_add_header(resp, k, v);

    if (!strings.casecmp(k, "Status")) {
        int status = atoi(v);
        resp->status = status;
    }

    lkalloc.lk_free(linetmp);
}
