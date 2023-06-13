
/*** LKHttpRequestParser ***/
typedef struct {
    LKString *partial_line;
    unsigned int nlinesread;
    int head_complete;              // flag indicating header lines complete
    int body_complete;              // flag indicating request body complete
    unsigned int content_length;    // value of Content-Length header
} LKHttpRequestParser;

typedef enum {
    LKHTTPSERVEROPT_HOMEDIR,
    LKHTTPSERVEROPT_PORT,
    LKHTTPSERVEROPT_HOST,
    LKHTTPSERVEROPT_CGIDIR,
    LKHTTPSERVEROPT_ALIAS,
    LKHTTPSERVEROPT_PROXYPASS
} LKHttpServerOpt;
