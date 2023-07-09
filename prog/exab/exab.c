#import fs
#import http
#import ioroutine
#import opt
#import os/io
#import strings
#import time
#import table.c
#import stats

typedef {
    io.handle_t *connection; // Connection with the server
    io.buf_t *outbuf; // Request copy to send
    io.buf_t *inbuf; // Response to read
    int body_bytes_to_read; // How many bytes to read to get the full response.
    request_stats_t *stats; // Current request's stats
} connection_t;

typedef {
    time.t time_begin;
    time.t time_begin_writing;
    time.t time_begin_reading;
    time.t time_finish_reading;
} request_stats_t;

request_stats_t *request_stats = NULL;

#define MAX_CONCURRENCY 20000


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
char *user_agent = "ex-ApacheBench/0.0";
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
 * Byte counters.
 */
size_t total_received = 0;
size_t total_sent = 0;

/*
 * Error counters.
 */
int failed_connects = 0;
int read_errors = 0;
int write_errors = 0;
int error_responses = 0; // non-200

/*
 * Response info.
 */
char servername[1024] = {0};  /* name that server reports */
size_t response_length = 0;
char response_version[20] = {0};

/*
 * Report table.
 */
table.t REPORT = {};

// ----

int heartbeatres = 100; /* How often do we say we're alive */

/* Number of multiple requests to make */
size_t concurrency = 1;

/* time limit in secs */
size_t tlimit = 0;

bool keepalive = false;      /* try and do keepalive connections */

char *postdata = NULL;         /* *buffer containing data from postfile */
size_t postlen = 0; /* length of data to be POSTed */
char * colonhost = "";
int doneka = 0;            /* number of keep alive connections requests_done */
int bad = 0;
int err_length = 0;        /* requests failed due to response length */
int err_except = 0;        /* requests failed due to exception */


time.t START_TIME = {};
io.buf_t REQUEST = {};

void usage(const char *progname) {
    opt.opt_usage(progname);
    fprintf(stderr, "Usage: %s [options] [http"
        "://]hostname[:port]/path\n", progname);

// request config
    fprintf(stderr, "    -T content-type Content-type header for POSTing, eg.\n");
    fprintf(stderr, "                    'application/x-www-form-urlencoded'\n");
    fprintf(stderr, "                    Default is 'text/plain'\n");
    fprintf(stderr, "    -p postfile     File containing data to POST. Remember also to set -T\n");

//
    fprintf(stderr, "    -H attribute    Add Arbitrary header line, eg. 'Accept-Encoding: gzip'\n");
    fprintf(stderr, "                    Inserted after all normal header lines. (repeatable)\n");
    fprintf(stderr, "    -A attribute    Add Basic WWW Authentication, the attributes\n");
    fprintf(stderr, "                    are a colon separated username and password.\n");
    fprintf(stderr, "    -V              Print version number and exit\n");
    fprintf(stderr, "    -h              Display usage information (this message)\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    // Orthogonal options
    opt.opt_size("n", "number of requests to perform", &requests_to_do);
    opt.opt_size("c", "number of concurrent requests", &concurrency);
    opt.opt_size("t", "time limit, number of seconds to wait for responses", &tlimit);
    opt.opt_bool("k", "use HTTP keep-alive", &keepalive);
    opt.opt_str("C", "cooke value, eg. session_id=123456", &cookie);

    // Request config
    char *methodstring = NULL;
    opt.opt_str("m", "request method (GET, POST, HEAD)", &methodstring);

    char *postfile = NULL;
    opt.opt_str("p", "postfile", &postfile);
    opt.opt_str("T", "POST body content type", &content_type);

    // Hmm
    bool hflag = false;
    opt.opt_bool("h", "print usage", &hflag);

    bool vflag = false;
    opt.opt_bool("V", "print version", &vflag);

    opt.opt_str("A", "basic auth string (base64)", &autharg);


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
    if (hflag) {
        usage(argv[0]);
    }
    if (vflag) {
        printf("This is ex-ApacheBench, rewritten\n");
        return 0;
    }

    if (concurrency > MAX_CONCURRENCY) {
        fprintf(stderr, "concurrency is too high (%zu > %d)\n", concurrency, MAX_CONCURRENCY);
        return 1;
    }

    if (concurrency > requests_to_do) {
        fprintf(stderr, "cannot use concurrency level greater than total number of requests\n");
        return 1;
    }

    if (heartbeatres && requests_to_do > 150) {
        heartbeatres = requests_to_do / 10;   /* Print line every 10% of requests */
        // but never more often than once every 100
        if (heartbeatres < 100) heartbeatres = 100;
    } else {
        heartbeatres = 0;
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

    test();
    output_results(0);
    return 0;
}

void build_request(const char *url) {
    // Parse the URL.
    http.url_t u = {};
    if (!http.parse_url(&u, url)) {
        fprintf(stderr, "%s: invalid URL\n", url);
        exit(1);
    }
    if (!strcmp(u.schema, "https")) {
        fprintf(stderr, "SSL not compiled in; no https support\n");
        exit(1);
    }
    if (u.port[0] == '\0') {
        strcpy(u.port, "80");
    }
    colonhost = strings.newstr("%s:%s", u.hostname, u.port);

    table.add(&REPORT, "Server Hostname", "%s", u.hostname);
    table.add(&REPORT, "Server Port", "%s", u.port);
    table.add(&REPORT, "Document Path", "%s", u.path);

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

    // Set various headers.
    if (keepalive) {
        http.set_header(&req, "Connection", "Keep-Alive");
    }
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
        int l;
        char tmp[1024];
        // assume username passwd already to be in colon separated form.
        // Ready to be uu-encoded.
        char *optarg = autharg;
        while (isspace(*optarg)) {
            optarg++;
        }
        if (apr_base64_encode_len(strlen(optarg)) > sizeof(tmp)) {
            err("Authentication credentials too long\n");
        }
        l = apr_base64_encode(tmp, optarg, strlen(optarg));
        tmp[l] = '\0';
        http.set_header(&req, "Authorization", strings.newstr("Basic %s", tmp));
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

void test() {
    request_stats = calloc(requests_to_do, sizeof(request_stats_t));
    connection_t *connections = calloc(concurrency, sizeof(connection_t));
    if (!connections || !request_stats) {
        panic("failed to allocate memory");
    }

    ioroutine.init();

    if (!tlimit) {
        tlimit = INT_MAX;
    }


    for (size_t i = 0; i < concurrency; i++) {
        connections[i].outbuf = io.newbuf();
        connections[i].inbuf = io.newbuf();
        ioroutine.spawn(routine, &connections[i]);
    }

    START_TIME = time.now();
    time.t stoptime = time.add(START_TIME, tlimit, time.SECONDS);
    while (ioroutine.step()) {
        if (time.sub(time.now(), stoptime) > 0) {
            break;
        }
    }
    if (heartbeatres) {
        fprintf(stderr, "Finished %zu requests\n", requests_done);
    } else {
        printf("..requests_done\n");
    }
}

enum {
    CONNECT,
    INIT_REQUEST,
    WRITE_REQUEST,
    READ_RESPONSE,
    READ_RESPONSE_BODY,
};

int routine(void *ctx, int line) {
    connection_t *c = ctx;

    switch (line) {
        /*
         * Connect to the remote host.
         */
        case CONNECT: {
            // TODO: connect timeout
            dbg("Connecting to %s", colonhost);
            c->connection = io.connect("tcp", colonhost);
            if (!c->connection) {
                fprintf(stderr, "failed to connect to %s: %s\n", colonhost, strerror(errno));
                failed_connects++;
                if (bad++ > 10) {
                    fatal("Test aborted after 10 failures");
                }
                return CONNECT;
            }
            dbg("connected");
            return INIT_REQUEST;
        }
        /*
         * Prepare a new request for writing.
         */
        case INIT_REQUEST: {
            if (requests_done >= requests_to_do) {
                printf("all done\n");
                io.close(c->connection);
                return -1;
            }
            c->stats = &request_stats[requests_done];
            requests_done++;
            io.resetbuf(c->inbuf);
            io.resetbuf(c->outbuf);
            dbg("preparing the request (%zu, %zu)", io.bufsize(c->inbuf), io.bufsize(c->outbuf));
            io.push(c->outbuf, REQUEST.data, io.bufsize(&REQUEST));
            c->stats->time_begin_writing = time.now();
            return WRITE_REQUEST;
        }
        /*
         * Write the request.
         */
        case WRITE_REQUEST: {
            size_t n = io.bufsize(c->outbuf);
            if (n == 0) {
                dbg("nothing more to write, proceeding to read");
                c->stats->time_begin_reading = time.now();
                return READ_RESPONSE;
            }
            if (!ioroutine.ioready(c->connection, io.WRITE)) {
                return WRITE_REQUEST;
            }
            dbg("writing %zu bytes\n", n);
            if (!io.write(c->connection, c->outbuf)) {
                write_errors++;
                printf("Send request failed!\n");
                io.close(c->connection);
                return CONNECT;
            }
            total_sent += n - io.bufsize(c->outbuf);
            return WRITE_REQUEST;
        }
        /*
         * Read and parse a response
         */
        case READ_RESPONSE: {
            if (!ioroutine.ioready(c->connection, io.READ)) {
                return READ_RESPONSE;
            }
            dbg("reading response");
            size_t n0 = io.bufsize(c->inbuf);
            if (!io.read(c->connection, c->inbuf)) {
                read_errors++;
                bad++;
                fprintf(stderr, "socket read failed: %s\n", strerror(errno));
                io.close(c->connection);
                return CONNECT;
            }
            size_t n = io.bufsize(c->inbuf);
            dbg("read %zu bytes", n - n0);
            total_received += n - n0;
            if (n == 0) {
                io.close(c->connection);
                c->stats->time_finish_reading = time.now();
                if (heartbeatres && !(requests_done % heartbeatres)) {
                    fprintf(stderr, "Completed %ld requests\n", requests_done);
                    fflush(stderr);
                }
                return CONNECT;
            }

            http.response_t r = {};
            if (!http.parse_response(c->inbuf->data, &r)) {
                panic("failed to parse the response\n");
                return READ_RESPONSE;
            }

            strcpy(servername, r.servername);
            response_length = r.head_length + r.content_length;
            strcpy(response_version, r.version);

            // printf("version = %s\n", r.version);
            // printf("status = %d\n", r.status);
            // printf("status_text = %s\n", r.status_text);
            // printf("servername = %s\n", r.servername);
            // printf("head_length = %d\n", r.head_length);
            // printf("content_length = %d\n", r.content_length);

            io.shift(c->inbuf, r.head_length);
            c->body_bytes_to_read = r.content_length;
            c->body_bytes_to_read -= io.bufsize(c->inbuf);
            io.shift(c->inbuf, io.bufsize(c->inbuf));

            if (r.status < 200 || r.status >= 300) {
                error_responses++;
                fprintf(stderr, "WARNING: Response code not 2xx (%d)\n", r.status);
            }
            return READ_RESPONSE_BODY;
        }
        case READ_RESPONSE_BODY: {
            if (c->body_bytes_to_read == 0) {
                return INIT_REQUEST;
            }
            if (!ioroutine.ioready(c->connection, io.READ)) {
                return READ_RESPONSE_BODY;
            }
            if (!io.read(c->connection, c->inbuf)) {
                panic("read failed");
            }
            size_t n = io.bufsize(c->inbuf);
            printf("read %zu of body\n", n);
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
    time.t now = time.now();
    double timetaken = (double)time.sub(now, START_TIME) / time.SECONDS;

    printf("\n\n");
    table.add(&REPORT, "Server Software", "%s", servername);
    table.add(&REPORT, "Response size", "%zu B", response_length);
    table.add(&REPORT, "HTTP version", "%s", response_version);
    table.split(&REPORT);
    table.add(&REPORT, "Concurrency Level", "%zu", concurrency);
    table.add(&REPORT, "Complete requests", "%ld", requests_done);
    table.add(&REPORT, "Failed requests", "%d", bad);
    table.add(&REPORT, "Connect errors", "%d", failed_connects);
    table.add(&REPORT, "Receive errors", "%d", read_errors);
    table.add(&REPORT, "Length errors", "%d", err_length);
    table.add(&REPORT, "Exceptions", "%d", err_except);
    table.add(&REPORT, "Write errors", "%d", write_errors);
    table.add(&REPORT, "Non-2xx responses", "%d", error_responses);
    table.add(&REPORT, "Keep-Alive requests", "%d", doneka);

    table.split(&REPORT);
    table.add(&REPORT, "Total received", "%ld B", total_received);
    table.add(&REPORT, "Total sent", "%ld B", total_sent);

    table.split(&REPORT);
    table.add(&REPORT, "Time taken for tests", "%.2f s", timetaken);
    table.add(&REPORT, "Requests per second", "%.2f", requests_done / timetaken);
    table.add(&REPORT, "Time per request", "%.3f ms", (double) concurrency * timetaken * 1000 / requests_done);
    table.add(&REPORT, "Time per request", "%.3f (across all concurrent requests)", (double) timetaken * 1000 / requests_done);
    table.add(&REPORT, "Receive rate", "%.2f KB/s", (double) total_received / 1024 / timetaken);
    table.add(&REPORT, "Send rate", "%.2f KB/s", (double) total_sent / 1024 / timetaken);
    table.print(&REPORT);

    if (requests_done > 0) {
        print_stats();
    }
    if (sig) {
        exit(1);
    }
}

void print_stats() {
    stats.series_t *write_times = stats.newseries();
    stats.series_t *read_times = stats.newseries();
    stats.series_t *sum_times = stats.newseries();

    for (size_t i = 0; i < requests_done; i++) {
        request_stats_t *s = &request_stats[i];
        stats.add(write_times, (double) (time.sub(s->time_begin_reading, s->time_begin_writing)) );
        stats.add(read_times, (double) (time.sub(s->time_finish_reading, s->time_begin_reading)) );
        stats.add(sum_times, (double) (time.sub(s->time_finish_reading, s->time_begin_writing)) );
    }

    printf("\nConnection Times (ms)\n");
    printf("              min  mean[+/-sd] median   max\n");
    printf("Writing: %5.1f %5.1f %5.1f %5.1f %5.1f\n",
        stats.min(write_times),
        stats.mean(write_times),
        stats.sd(write_times),
        stats.median(write_times),
        stats.max(write_times)
    );
    printf("Reading: %5.1f %5.1f %5.1f %5.1f %5.1f\n",
        stats.min(read_times),
        stats.mean(read_times),
        stats.sd(read_times),
        stats.median(read_times),
        stats.max(read_times)
    );
    printf("Sum: %5.1f %5.1f %5.1f %5.1f %5.1f\n",
        stats.min(sum_times),
        stats.mean(sum_times),
        stats.sd(sum_times),
        stats.median(sum_times),
        stats.max(sum_times)
    );

    if (requests_done > 1) {
        int percs[] = {50, 66, 75, 80, 90, 95, 98, 99, 100};
        printf("\nRequest time percentiles\n");
        for (size_t i = 0; i < nelem(percs); i++) {
            int p = percs[i];
            if (p == 100) {
                printf(" 100%%  %.1f (longest request)\n", stats.max(sum_times));
            } else {
                printf("  %d%%  %.1f\n", p, stats.percentile(sum_times, p));
            }
        }
    }
}

void err(char *s) {
    fprintf(stderr, "%s\n", s);
    if (requests_done) {
        printf("Total of %ld requests completed\n" , requests_done);
    }
    exit(1);
}

size_t apr_base64_encode_len() {
    panic("todo");
}

int apr_base64_encode() {
    panic("todo");
}

void dbg(const char *format, ...) {
    (void) format;
    // printf("[dbg] ");
    // va_list l = {0};
	// va_start(l, format);
	// vprintf(format, l);
	// va_end(l);
    // printf("\n");
}
