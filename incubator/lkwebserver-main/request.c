#import lkstring.c
#import lkbuffer.c

pub typedef {
    lkstring.LKString *method;       // GET
    lkstring.LKString *uri;          // "/path/to/index.html?p=1&start=5"
    lkstring.LKString *path;         // "/path/to/index.html"
    lkstring.LKString *filename;     // "index.html"
    lkstring.LKString *querystring;  // "p=1&start=5"
    lkstring.LKString *version;      // HTTP/1.0
    LKStringTable *headers;
    lkbuffer.LKBuffer *head;
    lkbuffer.LKBuffer *body;
} LKHttpRequest;


pub typedef {
    int status;             // 404
    lkstring.LKString *statustext;    // File not found
    lkstring.LKString *version;       // HTTP/1.0
    LKStringTable *headers;
    lkbuffer.LKBuffer *head;
    lkbuffer.LKBuffer *body;
} LKHttpResponse;
