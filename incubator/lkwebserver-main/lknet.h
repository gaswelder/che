

/*** LKHttpRequest - HTTP Request struct ***/
typedef struct {
    LKString *method;       // GET
    LKString *uri;          // "/path/to/index.html?p=1&start=5"
    LKString *path;         // "/path/to/index.html"
    LKString *filename;     // "index.html"
    LKString *querystring;  // "p=1&start=5"
    LKString *version;      // HTTP/1.0
    LKStringTable *headers;
    LKBuffer *head;
    LKBuffer *body;
} LKHttpRequest;

LKHttpRequest *lk_httprequest_new();
void lk_httprequest_free(LKHttpRequest *req);
void lk_httprequest_add_header(LKHttpRequest *req, char *k, char *v);
void lk_httprequest_append_body(LKHttpRequest *req, char *bytes, int bytes_len);
void lk_httprequest_finalize(LKHttpRequest *req);
void lk_httprequest_debugprint(LKHttpRequest *req);


/*** LKHttpResponse - HTTP Response struct ***/
typedef struct {
    int status;             // 404
    LKString *statustext;    // File not found
    LKString *version;       // HTTP/1.0
    LKStringTable *headers;
    LKBuffer *head;
    LKBuffer *body;
} LKHttpResponse;

LKHttpResponse *lk_httpresponse_new();
void lk_httpresponse_free(LKHttpResponse *resp);
void lk_httpresponse_add_header(LKHttpResponse *resp, char *k, char *v);
void lk_httpresponse_finalize(LKHttpResponse *resp);
void lk_httpresponse_debugprint(LKHttpResponse *resp);


/*** LKSocketReader - Buffered input for sockets ***/
typedef struct {
    int sock;
    LKBuffer *buf;
    int sockclosed;
} LKSocketReader;

LKSocketReader *lk_socketreader_new(int sock, size_t initial_size);
void lk_socketreader_free(LKSocketReader *sr);
int lk_socketreader_readline(LKSocketReader *sr, LKString *line);
int lk_socketreader_recv(LKSocketReader *sr, LKBuffer *buf);
void lk_socketreader_debugprint(LKSocketReader *sr);


/*** LKHttpRequestParser ***/
typedef struct {
    LKString *partial_line;
    unsigned int nlinesread;
    int head_complete;              // flag indicating header lines complete
    int body_complete;              // flag indicating request body complete
    unsigned int content_length;    // value of Content-Length header
} LKHttpRequestParser;

LKHttpRequestParser *lk_httprequestparser_new();
void lk_httprequestparser_free(LKHttpRequestParser *parser);
void lk_httprequestparser_reset(LKHttpRequestParser *parser);
void lk_httprequestparser_parse_line(LKHttpRequestParser *parser, LKString *line, LKHttpRequest *req);
void lk_httprequestparser_parse_bytes(LKHttpRequestParser *parser, LKBuffer *buf, LKHttpRequest *req);

/*** CGI Parser ***/
void parse_cgi_output(LKBuffer *buf, LKHttpResponse *resp);


/*** LKContext ***/
typedef enum {
    CTX_READ_REQ,
    CTX_READ_CGI_OUTPUT,
    CTX_WRITE_CGI_INPUT,
    CTX_WRITE_RESP,
    CTX_PROXY_WRITE_REQ,
    CTX_PROXY_PIPE_RESP,
} LKContextType;

typedef struct lkcontext_s {
    int selectfd;
    int clientfd;
    LKContextType type;
    struct lkcontext_s *next;         // link to next ctx

    // Used by CTX_READ_REQ:
    struct sockaddr_in client_sa;     // client address
    LKString *client_ipaddr;          // client ip address string
    unsigned short client_port;       // client port number
    LKString *req_line;               // current request line
    LKBuffer *req_buf;                // current request bytes buffer
    LKSocketReader *sr;               // input buffer for reading lines
    LKHttpRequestParser *reqparser;   // parser for httprequest
    LKHttpRequest *req;               // http request in process

    // Used by CTX_WRITE_REQ:
    LKHttpResponse *resp;             // http response to be sent
    LKRefList *buflist;               // Buffer list of things to send/recv

    // Used by CTX_READ_CGI:
    int cgifd;
    LKBuffer *cgi_outputbuf;          // receive cgi stdout bytes here
    LKBuffer *cgi_inputbuf;           // input bytes to pass to cgi stdin

    // Used by CTX_PROXY_WRITE_REQ:
    int proxyfd;
    LKBuffer *proxy_respbuf;
} LKContext;

LKContext *lk_context_new();
LKContext *create_initial_context(int fd, struct sockaddr_in *sa);
void lk_context_free(LKContext *ctx);

void add_new_client_context(LKContext **pphead, LKContext *ctx);
void add_context(LKContext **pphead, LKContext *ctx);
int remove_client_context(LKContext **pphead, int clientfd);
void remove_client_contexts(LKContext **pphead, int clientfd);
LKContext *match_select_ctx(LKContext *phead, int selectfd);
int remove_selectfd_context(LKContext **pphead, int selectfd);


LKConfig *lk_config_new();
void lk_config_free(LKConfig *cfg);
int lk_config_read_configfile(LKConfig *cfg, char *configfile);
void lk_config_print(LKConfig *cfg);
LKHostConfig *lk_config_add_hostconfig(LKConfig *cfg, LKHostConfig *hc);
LKHostConfig *lk_config_find_hostconfig(LKConfig *cfg, char *hostname);
LKHostConfig *lk_config_create_get_hostconfig(LKConfig *cfg, char *hostname);
void lk_config_finalize(LKConfig *cfg);

LKHostConfig *lk_hostconfig_new(char *hostname);
void lk_hostconfig_free(LKHostConfig *hc);




typedef enum {
    LKHTTPSERVEROPT_HOMEDIR,
    LKHTTPSERVEROPT_PORT,
    LKHTTPSERVEROPT_HOST,
    LKHTTPSERVEROPT_CGIDIR,
    LKHTTPSERVEROPT_ALIAS,
    LKHTTPSERVEROPT_PROXYPASS
} LKHttpServerOpt;



// Function return values:
// Z_OPEN (fd still open)
// Z_EOF (end of file)
// Z_ERR (errno set with error detail)
// Z_BLOCK (fd blocked, no data)
#define Z_OPEN 1
#define Z_EOF 0
#define Z_ERR -1
#define Z_BLOCK -2

typedef enum {FD_SOCK, FD_FILE} FDType;
typedef enum {FD_READ, FD_WRITE, FD_READWRITE} FDAction;

// Read nonblocking fd count bytes to buf.
// Returns one of the following:
//    1 (Z_OPEN) for socket open (data available)
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
// On return, nbytes contains the number of bytes read.
int lk_read(int fd, FDType fd_type, LKBuffer *buf, size_t count, size_t *nbytes);
int lk_read_sock(int fd, LKBuffer *buf, size_t count, size_t *nbytes);
int lk_read_file(int fd, LKBuffer *buf, size_t count, size_t *nbytes);

// Read all available nonblocking fd bytes to buffer.
// Returns one of the following:
//    0 (Z_EOF) for EOF
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
//
// Note: This keeps track of last buf position read.
// Used to cumulatively read data into buf.
int lk_read_all(int fd, FDType fd_type, LKBuffer *buf);
int lk_read_all_sock(int fd, LKBuffer *buf);
int lk_read_all_file(int fd, LKBuffer *buf);

// Write count buf bytes to nonblocking fd.
// Returns one of the following:
//    1 (Z_OPEN) for socket open
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
// On return, nbytes contains the number of bytes written.
int lk_write(int fd, FDType fd_type, LKBuffer *buf, size_t count, size_t *nbytes);
int lk_write_sock(int fd, LKBuffer *buf, size_t count, size_t *nbytes);
int lk_write_file(int fd, LKBuffer *buf, size_t count, size_t *nbytes);

// Write buf bytes to nonblocking fd.
// Returns one of the following:
//    1 (Z_OPEN) for socket open
//   -1 (Z_ERR) for error
//   -2 (Z_BLOCK) for blocked socket (no data)
//
// Note: This keeps track of last buf position written.
// Used to cumulatively write data into buf.
int lk_write_all(int fd, FDType fd_type, LKBuffer *buf);
int lk_write_all_sock(int fd, LKBuffer *buf);
int lk_write_all_file(int fd, LKBuffer *buf);

// Similar to lk_write_all(), but sending buflist buf's sequentially.
int lk_buflist_write_all(int fd, FDType fd_type, LKRefList *buflist);

// Pipe all available nonblocking readfd bytes into writefd.
// Uses buf as buffer for queued up bytes waiting to be written.
// Returns one of the following:
//    0 (Z_EOF) for read/write complete.
//    1 (Z_OPEN) for writefd socket open
//   -1 (Z_ERR) for read/write error.
//   -2 (Z_BLOCK) for blocked readfd/writefd socket
int lk_pipe_all(int readfd, int writefd, FDType fd_type, LKBuffer *buf);

// Remove trailing CRLF or LF (\n) from string.
void lk_chomp(char* s);
// Read entire file into buf.
ssize_t lk_readfile(char *filepath, LKBuffer *buf);
// Read entire file descriptor contents into buf.
ssize_t lk_readfd(int fd, LKBuffer *buf);
// Append src to dest, allocating new memory in dest if needed.
// Return new pointer to dest.
char *lk_astrncat(char *dest, char *src, size_t src_len);
// Return whether file exists.
int lk_file_exists(char *filename);
