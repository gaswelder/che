#import dbg
#import fs
#import http
#import ioroutine
#import opt
#import os/io
#import strings
#import time

typedef {
    io.handle_t *connection; // Connection with the server
    io.buf_t *outbuf; // Request copy to send
    io.buf_t *inbuf; // Response to read
    int body_bytes_to_read; // How many bytes to read to get the full response.
    request_stats_t *stats; // Current request's stats
} connection_t;

typedef {
    time.t time_started;
    time.t time_finished_writing;
    time.t time_finished_reading;
	int status;
	size_t total_received;
	size_t total_sent;
	int error;
} request_stats_t;

request_stats_t *request_stats = NULL;

// Request method.
enum {
    GET,
    POST,
    HEAD
};
int method = GET;

/*
 * Customizable headers
 */
char *user_agent = "exab";
char *opt_accept = NULL;
char *cookie = NULL;
char *content_type = "text/plain";
char *autharg = NULL;

/*
 * How many requests to do.
 */
size_t requests_to_do = 1;
size_t requests_done = 0;

/*
 * How many requests to do in parallel.
 */
#define MAX_CONCURRENCY 20000
size_t concurrency = 1;

/*
 * Response info.
 */
char servername[1024] = {0};  /* name that server reports */
size_t response_length = 0;
char response_version[20] = {0};

/*
 * Other state.
 */
time.t START_TIME = {};
io.buf_t REQUEST = {};

// ----

char *postdata = NULL;         /* *buffer containing data from postfile */
size_t postlen = 0; /* length of data to be POSTed */
char *colonhost = "";

int _bad = 0;
void bad() {
	_bad++;
	if (_bad > 10) {
		fatal("Test aborted after 10 failures");
	}
}

int main(int argc, char *argv[]) {
    opt.opt_summary("exab [options] <url> - makes multiple HTTP requests to <url> and prints statistics");
    // Orthogonal options
    opt.opt_size("n", "number of requests to perform (1)", &requests_to_do);
    opt.opt_size("c", "number of requests running concurrently (1)", &concurrency);
    opt.opt_str("C", "cooke value, eg. session_id=123456", &cookie);
    opt.opt_str("a", "HTTP basic auth value (username:password)", &autharg);

    // Request config
    char *methodstring = NULL;
    opt.opt_str("m", "request method (GET, POST, HEAD)", &methodstring);

    char *postfile = NULL;
    opt.opt_str("p", "path to the file with the POST body", &postfile);
    opt.opt_str("T", "POST body content type (default is text/plain)", &content_type);

    bool hflag = false;
    opt.opt_bool("h", "print usage", &hflag);

    char **args = opt.opt_parse(argc, argv);

    if (methodstring) {
        if (strings.casecmp(methodstring, "GET")) {
            method = GET;
        } else if (strings.casecmp(methodstring, "POST")) {
            method = POST;
        } else if (strings.casecmp(methodstring, "HEAD")) {
            method = HEAD;
        } else {
            fprintf(stderr, "unknown request method: %s\n", methodstring);
            return 1;
        }
    }

    if (postfile) {
        if (method != POST) {
            fprintf(stderr, "postfile option works only with POST method\n");
            exit(1);
        }
        postdata = fs.readfile(postfile, &postlen);
        if (!postdata) {
            fprintf(stderr, "failed to read file %s: %s\n", postfile, strerror(errno));
            exit(1);
        }
    }
    if (hflag) return opt.usage();

    if (concurrency > MAX_CONCURRENCY) {
        fprintf(stderr, "concurrency is too high (%zu > %d)\n", concurrency, MAX_CONCURRENCY);
        return 1;
    }

    if (concurrency > requests_to_do) {
        fprintf(stderr, "cannot use concurrency level greater than total number of requests\n");
        return 1;
    }

    if (!*args) {
        fprintf(stderr, "missing the url argument\n");
        return 1;
    }
    char *url = *args;
    args++;
    if (*args) {
        fprintf(stderr, "unexpected argument: %s\n", *args);
        return 1;
    }

    build_request(url);

    /* Output the results if the user terminates the run early. */
    signal(SIGINT, output_results);

	request_stats = calloc(requests_to_do, sizeof(request_stats_t));
    connection_t *connections = calloc(concurrency, sizeof(connection_t));
    if (!connections || !request_stats) {
        panic("failed to allocate memory");
    }
	ioroutine.init();
	for (size_t i = 0; i < concurrency; i++) {
        connections[i].outbuf = io.newbuf();
        connections[i].inbuf = io.newbuf();
        ioroutine.spawn(routine, &connections[i]);
    }
	START_TIME = time.now();
    while (ioroutine.step()) {
        //
    }
    output_results(0);
    return 0;
}

void build_request(const char *url) {
    // Parse the URL.
    http.url_t u = {};
    if (!http.parse_url(&u, url)) {
        fprintf(stderr, "invalid URL: %s\n", url);
        exit(1);
    }
    if (!strcmp(u.schema, "https")) {
        fprintf(stderr, "no https support\n");
        exit(1);
    }
    if (u.port[0] == '\0') {
        strcpy(u.port, "80");
    }
    colonhost = strings.newstr("%s:%s", u.hostname, u.port);

    // Create a request.
    http.request_t req = {};
    const char *methodstring = NULL;
    switch (method) {
        case GET: { methodstring = "GET"; }
        case POST: { methodstring = "POST"; }
        case HEAD: { methodstring = "HEAD"; }
        default: { panic("!"); }
    }
    http.init_request(&req, methodstring, u.path);
    http.set_header(&req, "Host", u.hostname);

    if (method == POST) {
        char buf[10] = {0};
        sprintf(buf, "%zu", postlen);
        http.set_header(&req, "Content-Length", buf);
        http.set_header(&req, "Content-Type", content_type);
    }
    http.set_header(&req, "User-Agent", user_agent);
    if (opt_accept) {
        http.set_header(&req, "Accept", opt_accept);
    }
    if (cookie) {
        http.set_header(&req, "Cookie", cookie);
    }
    if (autharg) {
        // assume username passwd already to be in colon separated form.
        // Ready to be uu-encoded.
        panic("base64 encoding not implemented");
        // http.set_header(&req, "Authorization", strings.newstr("Basic %s", tmp));
    }
    char buf[1000] = {0};
    if (!http.write_request(&req, buf, sizeof(buf))) {
        fatal("failed to write request");
    }
    io.push(&REQUEST, buf, strlen(buf));
    if (method == POST) {
        io.push(&REQUEST, postdata, strlen(postdata));
    }
}

enum {
    CONNECT,
    INIT_REQUEST,
    WRITE_REQUEST,
    READ_RESPONSE,
    READ_RESPONSE_BODY,
};

const char *DBG_TAG = "exab";

int routine(void *ctx, int line) {
    connection_t *c = ctx;

    switch (line) {
        /*
         * Connect to the remote host.
         */
        case CONNECT: {
			c->stats = &request_stats[requests_done++];
            // TODO: connect timeout
            dbg.m(DBG_TAG, "Connecting to %s", colonhost);
            c->connection = io.connect("tcp", colonhost);
            if (!c->connection) {
				c->stats->error = errno;
				bad();
                return CONNECT;
            }
            dbg.m(DBG_TAG, "connected");
            return INIT_REQUEST;
        }
        /*
         * Prepare a new request for writing.
         */
        case INIT_REQUEST: {
            if (requests_done >= requests_to_do) {
                dbg.m(DBG_TAG, "all done\n");
                io.close(c->connection);
                return -1;
            }
            io.resetbuf(c->inbuf);
            io.resetbuf(c->outbuf);
            dbg.m(DBG_TAG, "preparing the request (%zu, %zu)", io.bufsize(c->inbuf), io.bufsize(c->outbuf));
            io.push(c->outbuf, REQUEST.data, io.bufsize(&REQUEST));
            c->stats->time_started = time.now();
            return WRITE_REQUEST;
        }
        /*
         * Write the request.
         */
        case WRITE_REQUEST: {
            size_t n = io.bufsize(c->outbuf);
            if (n == 0) {
                dbg.m(DBG_TAG, "nothing more to write, proceeding to read");
                c->stats->time_finished_writing = time.now();
                return READ_RESPONSE;
            }
            if (!ioroutine.ioready(c->connection, io.WRITE)) {
                return WRITE_REQUEST;
            }
            dbg.m(DBG_TAG, "writing %zu bytes\n", n);
            if (!io.write(c->connection, c->outbuf)) {
				c->stats->error = errno;
                io.close(c->connection);
                c->stats->time_finished_writing = time.now();
                return CONNECT;
            }
            c->stats->total_sent += n - io.bufsize(c->outbuf);
            return WRITE_REQUEST;
        }
        /*
         * Read and parse the response.
         */
        case READ_RESPONSE: {
            if (!ioroutine.ioready(c->connection, io.READ)) {
                return READ_RESPONSE;
            }
            dbg.m(DBG_TAG, "reading response");
            size_t n0 = io.bufsize(c->inbuf);
            if (!io.read(c->connection, c->inbuf)) {
				c->stats->error = errno;
                io.close(c->connection);
                c->stats->time_finished_reading = time.now();
				bad();
                return CONNECT;
            }
            size_t n = io.bufsize(c->inbuf);
            dbg.m(DBG_TAG, "read %zu bytes", n - n0);
            c->stats->total_received += n - n0;
            if (n == 0) {
                io.close(c->connection);
                c->stats->time_finished_reading = time.now();
                return CONNECT;
            }

            http.response_t r = {};
            if (!http.parse_response(c->inbuf->data, &r)) {
                // Assume not enough data was received.
                dbg.m(DBG_TAG, "waiting for more headers");
                return READ_RESPONSE;
            }
			c->stats->status = r.status;
            strcpy(servername, r.servername);
            response_length = r.head_length + r.content_length;
            strcpy(response_version, r.version);
            io.shift(c->inbuf, r.head_length);
            c->body_bytes_to_read = r.content_length;
            c->body_bytes_to_read -= io.bufsize(c->inbuf);
            io.shift(c->inbuf, io.bufsize(c->inbuf));
            return READ_RESPONSE_BODY;
        }
        /*
         * Read the rest of the body.
         */
        case READ_RESPONSE_BODY: {
            if (c->body_bytes_to_read == 0) {
                c->stats->time_finished_reading = time.now();
                return INIT_REQUEST;
            }
            if (!ioroutine.ioready(c->connection, io.READ)) {
                return READ_RESPONSE_BODY;
            }
            if (!io.read(c->connection, c->inbuf)) {
                panic("read failed");
            }
            size_t n = io.bufsize(c->inbuf);
            dbg.m(DBG_TAG, "read %zu of body\n", n);
            c->body_bytes_to_read -= n;
            io.shift(c->inbuf, n);
            return READ_RESPONSE_BODY;
        }

        default: {
            panic("unexpected line: %d", line);
        }
    }
    return -1;
}


void fatal(char *s) {
    fprintf(stderr, "%s: %s (%d)\n", s, strerror(errno), errno);
    if (requests_done) {
        printf("Total of %zu requests completed\n" , requests_done);
    }
    exit(1);
}

void output_results(int sig) {
	printf("#\tstatus\twriting\treading\ttotal\tsent\treceived\n");
    for (size_t i = 0; i < requests_done; i++) {
        request_stats_t *s = &request_stats[i];
        int64_t write = time.sub(s->time_finished_writing, s->time_started);
        int64_t read = time.sub(s->time_finished_reading, s->time_finished_writing);
        int64_t total = time.sub(s->time_finished_reading, s->time_started);
		printf("%zu\t", i);
		if (s->error) {
			printf("error: %d (%s)\n", s->error, strerror(s->error));
			continue;
		}
		printf("%d\t", s->status);
		printf("%f\t", (double) write / time.MS);
		printf("%f\t", (double) read / time.MS);
		printf("%f\t", (double) total / time.MS);
		printf("%zu\t", s->total_sent);
		printf("%zu\n", s->total_received);
    }
    if (sig) exit(1);
}
