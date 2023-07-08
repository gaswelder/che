#import fs
#import http
#import ioroutine
#import opt
#import os/io
#import strings
#import time

/* Note: this version string should start with \d+[\d\.]* and be a valid
 * string for an HTTP Agent: header when prefixed with 'ApacheBench/'.
 * It should reflect the version of AB - and not that of the apache server
 * it happens to accompany. And it should be updated or changed whenever
 * the results are no longer fundamentally comparable to the results of
 * a previous version of ab. Either due to a change in the logic of
 * ab - or to due to a change in the distribution it is compiled with
 * (such as an APR change in for example blocking).
 */
#define AP_AB_BASEREVISION "2.3"

/*
 * BUGS:
 *
 * - uses strcpy/etc.
 * - has various other poor buffer attacks related to the lazy parsing of
 *   response headers from the server
 * - doesn't implement much of HTTP/1.x, only accepts certain forms of
 *   responses
 * - (performance problem) heavy use of strstr shows up top in profile
 *   only an issue for loopback usage
 */

enum {
    MAX_REQUESTS = 50000
};

enum {
    STATE_UNCONNECTED = 0,
    STATE_CONNECTING,           /* TCP connect initiated, but we don't
                                 * know if it worked yet
                                 */
    STATE_CONNECTED,            /* we know TCP connect completed */
    STATE_READ
};

#define CBUFFSIZE (2048)

typedef {
    io.handle_t *aprsock; // Connection with the server
    io.buf_t *outbuf; // Request copy to send
    io.buf_t *inbuf; // Response to read

    int body_bytes_to_read; // How many bytes to read to get the full response.


    int state;
    size_t read;            /* amount of bytes read */
    size_t bread;           /* amount of body read */
    size_t rwrite, rwrote;  /* keep pointers in what we write - across
                                 * EAGAINs */
    size_t length;          /* Content-Length value used for keep-alive */
    char cbuff[CBUFFSIZE];      /* a buffer to store server response header */
    int cbx;                    /* offset in cbuffer */
    int keepalive;              /* non-zero if a keep-alive request */
    int gotheader;              /* non-zero if we have the entire header in
                                 * cbuff */
    time.t start,           /* Start of connection */
               connect,         /* Connected, start writing */
               endwrite,        /* Request written */
               beginread,       /* First byte of input */
               done;            /* Connection closed */
} connection_t;

typedef {
    time.t starttime;         /* start time of connection */
    int64_t waittime; /* between request and reading response */
    int64_t ctime;    /* time to connect */
    int64_t time;     /* time for connection */
} data_t;

double ap_double_ms(int a) {
    return ((double)(a)/1000.0);
}
#define MAX_CONCURRENCY 20000

/* --------------------- GLOBALS ---------------------------- */

int verbosity = 0;      /* no verbosity by default */
int posting = 0;        /* GET by default */
size_t requests = 1;       /* Number of requests to make */
int heartbeatres = 100; /* How often do we say we're alive */

/* Number of multiple requests to make */
size_t concurrency = 1;

/* time limit in secs */
size_t tlimit = 0;

int percentile = 1;     /* Show percentile served */
int confidence = 1;     /* Show confidence estimator and warnings */
bool keepalive = false;      /* try and do keepalive connections */
char servername[1024] = {0};  /* name that server reports */
char *hostname = NULL;         /* host name from URL */
char *path = NULL;             /* path name */
char *postdata = NULL;         /* *buffer containing data from postfile */
size_t postlen = 0; /* length of data to be POSTed */
char *content_type = "text/plain";/* content type to put in POST header */
int port = 0;        /* port number */
char *gnuplot = NULL;          /* GNUplot file */
char *csvperc = NULL;          /* CSV Percentile file */
char *fullurl = NULL;
char * colonhost = "";

char *user_agent = "ex-ApacheBench/0.0";
char *opt_accept = NULL;
char *cookie = NULL;
char *autharg = NULL;

size_t doclen = 0;     /* the length the document should be */
int64_t totalread = 0;    /* total number of bytes read */
int64_t totalbread = 0;   /* totoal amount of entity body read */
int64_t totalposted = 0;  /* total number of bytes posted, inc. headers */
size_t started = 0;           /* number of requests started, so no excess */
size_t done = 0;              /* number of requests we have done */
int doneka = 0;            /* number of keep alive connections done */
int good = 0;
int bad = 0;     /* number of good and bad requests */
int epipe = 0;             /* number of broken pipe writes */
int err_length = 0;        /* requests failed due to response length */
int err_conn = 0;          /* requests failed due to connection drop */
int err_recv = 0;          /* requests failed due to broken read */
int err_except = 0;        /* requests failed due to exception */
int err_response = 0;      /* requests with invalid or non-200 response */


time.t start = {};
time.t lasttime = {};

/* global request (and its length) */
io.buf_t REQUEST = {};


/* interesting percentiles */
int percs[] = {50, 66, 75, 80, 90, 95, 98, 99, 100};

data_t *stats = NULL;         /* data for each request */

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
    fprintf(stderr, "    -v verbosity    How much troubleshooting info to print\n");
    fprintf(stderr, "    -H attribute    Add Arbitrary header line, eg. 'Accept-Encoding: gzip'\n");
    fprintf(stderr, "                    Inserted after all normal header lines. (repeatable)\n");
    fprintf(stderr, "    -A attribute    Add Basic WWW Authentication, the attributes\n");
    fprintf(stderr, "                    are a colon separated username and password.\n");
    fprintf(stderr, "    -V              Print version number and exit\n");

    fprintf(stderr, "    -d              Do not show percentiles served table.\n");
    fprintf(stderr, "    -S              Do not show confidence estimators and warnings.\n");
    fprintf(stderr, "    -g filename     Output collected data to gnuplot format file.\n");
    fprintf(stderr, "    -e filename     Output CSV file with percentages served\n");
    fprintf(stderr, "    -h              Display usage information (this message)\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    // Orthogonal options
    opt.opt_size("n", "number of requests to perform", &requests);
    opt.opt_size("c", "number of concurrent requests", &concurrency);
    opt.opt_size("t", "time limit, number of seconds to wait for responses", &tlimit);
    opt.opt_bool("k", "use HTTP keep-alive", &keepalive);
    opt.opt_str("C", "cooke value, eg. session_id=123456", &cookie);

    // Hmm
    bool iflag = false;
    opt.opt_bool("i", "use HEAD requests", &iflag);

    bool quiet = false;
    opt.opt_bool("q", "quiet", &quiet);

    bool hflag = false;
    bool vflag = false;


    opt.opt_int("v", "verbosity", &verbosity);
    opt.opt_str("g", "gnuplot output file", &gnuplot);
    opt.opt_bool("h", "print usage", &hflag);
    opt.opt_bool("V", "print version", &vflag);





    bool dflag = false;
    opt.opt_bool("d", "percentile = 0", &dflag);
    bool sflag = false;
    opt.opt_bool("S", "confidence = 0", &sflag);

    opt.opt_str("e", "csvperc", &csvperc);


    char *postfile = NULL;
    opt.opt_str("p", "postfile", &postfile);


    opt.opt_str("A", "basic auth string (base64)", &autharg);
    opt.opt_str("T", "POST body content type", &content_type);

    char **args = opt.opt_parse(argc, argv);

    if (tlimit) {
        // need to size data array on something.
        requests = MAX_REQUESTS;
    }
    if (postfile) {
        if (posting != 0) {
            err("Cannot mix POST and HEAD\n");
        }
        postdata = fs.readfile(postfile, &postlen);
        if (!postdata) {
            fprintf(stderr, "failed to read file %s: %s\n", postfile, strerror(errno));
            exit(1);
        }
        posting = 1;
    }
    if (iflag) {
        if (posting == 1) {
            err("Cannot mix POST and HEAD\n");
        }
        posting = -1;
    }
    if (hflag) {
        usage(argv[0]);
    }
    if (vflag) {
        printf("This is ex-ApacheBench, rewritten\n");
        return 0;
    }
    if (dflag) {
        percentile = 0;
    }
    if (sflag) {
        confidence = 0;
    }
    if (quiet) {
        heartbeatres = 0;
    }

    if (concurrency > MAX_CONCURRENCY) {
        fprintf(stderr, "concurrency is too high (%zu > %d)\n", concurrency, MAX_CONCURRENCY);
        return 1;
    }

    if (concurrency > requests) {
        fprintf(stderr, "cannot use concurrency level greater than total number of requests\n");
        return 1;
    }

    if ((heartbeatres) && (requests > 150)) {
        heartbeatres = requests / 10;   /* Print line every 10% of requests */
        if (heartbeatres < 100)
            heartbeatres = 100; /* but never more often than once every 100
                                 * connections. */
    }
    else
        heartbeatres = 0;

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
    fullurl = strings.newstr("%s", url);

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
        colonhost = strings.newstr("%s:80", u.hostname);
    } else {
        colonhost = strings.newstr("%s:%s", u.hostname, u.port);
    }

    const char *method = NULL;
    if (posting == -1) {
        method = "HEAD";
    } else if (posting == 0) {
        method = "GET";
    } else {
        method = "POST";
    }

    http.request_t req = {};
    http.init_request(&req, method, u.path);
    http.set_header(&req, "Host", u.hostname);
    if (keepalive) {
        http.set_header(&req, "Connection", "Keep-Alive");
    }
    if (posting > 1) {
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
    if (posting == 1) {
        io.push(&REQUEST, postdata, strlen(postdata));
    }
    if (verbosity >= 2) {
        io.print_buf(&REQUEST);
    }
}

void test() {
    stats = calloc(requests, sizeof(data_t));

    ioroutine.init();


    start = lasttime = time.now();
    if (!tlimit) {
        tlimit = INT_MAX;
    }
    time.t stoptime = time.add(start, tlimit, time.SECONDS);

    connection_t *connections = calloc(concurrency, sizeof(connection_t));
    if (!connections) {
        panic("failed to allocate memory");
    }
    for (size_t i = 0; i < concurrency; i++) {
        connections[i].outbuf = io.newbuf();
        connections[i].inbuf = io.newbuf();
        ioroutine.spawn(routine, &connections[i]);
    }
    int c = 0;
    while (true) {
        if (c++ > 10) {
            break;
        }
        ioroutine.step();
        if (done > requests || time.sub(time.now(), stoptime) > 0) {
            break;
        }
    }
    if (heartbeatres) {
        fprintf(stderr, "Finished %zu requests\n", done);
    } else {
        printf("..done\n");
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
            c->aprsock = io.connect("tcp", colonhost);
            if (!c->aprsock) {
                fprintf(stderr, "failed to connect to %s: %s\n", colonhost, strerror(errno));
                err_conn++;
                if (bad++ > 10) {
                    fatal("Test aborted after 10 failures");
                }
                return CONNECT;
            }
            started++;
            c->connect = time.now();
            return INIT_REQUEST;
        }
        /*
         * Prepare a new request for writing.
         */
        case INIT_REQUEST: {
            c->read = 0;
            c->bread = 0;
            c->keepalive = 0;
            c->cbx = 0;
            c->gotheader = 0;
            c->rwrite = 0;
            c->start = time.now();
            io.resetbuf(c->inbuf);
            io.resetbuf(c->outbuf);
            io.push(c->outbuf, REQUEST.data, io.bufsize(&REQUEST));
            return WRITE_REQUEST;
        }
        /*
         * Write the request.
         */
        case WRITE_REQUEST: {
            // If nothing to write skip to reading.
            if (io.bufsize(c->outbuf) == 0) {
                c->beginread = time.now();
                return READ_RESPONSE;
            }
            if (!ioroutine.ioready(c->aprsock, io.WRITE)) {
                return WRITE_REQUEST;
            }
            printf("writing %zu\n", io.bufsize(c->outbuf));
            if (!io.write(c->aprsock, c->outbuf)) {
                epipe++;
                printf("Send request failed!\n");
                io.close(c->aprsock);
                return CONNECT;
            }
            printf("have %zu remaining\n", io.bufsize(c->outbuf));
            if (io.bufsize(c->outbuf) == 0) {
                printf("add request written\n");
                c->endwrite = time.now();
            }
            c->beginread = time.now();
            return READ_RESPONSE;
        }
        /*
         * Read and parse a response
         */
        case READ_RESPONSE: {
            if (!ioroutine.ioready(c->aprsock, io.READ)) {
                return READ_RESPONSE;
            }
            if (!io.read(c->aprsock, c->inbuf)) {
                err_recv++;
                bad++;
                if (verbosity >= 1) {
                    fprintf(stderr, "socket read failed: %s\n", strerror(errno));
                }
                io.close(c->aprsock);
                return CONNECT;
            }

            http.response_t r = {};
            if (!http.parse_response(c->inbuf->data, &r)) {
                printf("failed to parse response\n");
                io.print_buf(c->inbuf);
                return READ_RESPONSE;
            }

            printf("version = %s\n", r.version);
            printf("status = %d\n", r.status);
            printf("status_text = %s\n", r.status_text);
            printf("servername = %s\n", r.servername);
            printf("head_length = %d\n", r.head_length);
            printf("content_length = %d\n", r.content_length);

            io.shift(c->inbuf, r.head_length);
            c->body_bytes_to_read = r.content_length;
            c->body_bytes_to_read -= io.bufsize(c->inbuf);
            io.shift(c->inbuf, io.bufsize(c->inbuf));

            // if (verbosity >= 2) {
            //     io.print_buf(c->inbuf);
            // }
            if (r.status < 200 || r.status >= 300) {
                err_response++;
                if (verbosity >= 2) {
                    printf("WARNING: Response code not 2xx (%d)\n", r.status);
                }
            }
            if (keepalive && posting >= 0) {
                c->length = r.content_length;
            }

            return READ_RESPONSE_BODY;
        }
        case READ_RESPONSE_BODY: {
            printf("remaining body: %d\n", c->body_bytes_to_read);
            if (c->body_bytes_to_read == 0) {
                printf("starting a new request\n");
                return INIT_REQUEST;
            }
            if (!ioroutine.ioready(c->aprsock, io.READ)) {
                return READ_RESPONSE_BODY;
            }
            if (!io.read(c->aprsock, c->inbuf)) {
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
    kek(c);
    close_connection(c);
    return -1;
}


void close_connection(connection_t * c)
{
    if (c->read == 0 && c->keepalive) {
        /*
         * server has legitimately shut down an idle keep alive request
         */
        if (good)
            good--;     /* connection never happened */
    }
    else {
        if (good == 1) {
            /* first time here */
            doclen = c->bread;
        }
        else if (c->bread != doclen) {
            bad++;
            err_length++;
        }
        /* save out time */
        if (done < requests) {
            data_t *s = &stats[done++];
            c->done      = lasttime = time.now();
            s->starttime = c->start;
            s->ctime     = max(0, time.sub(c->connect, c->start));
            s->time      = max(0, time.sub(c->done, c->start));
            s->waittime  = max(0, time.sub(c->beginread, c->endwrite));
            if (heartbeatres && !(done % heartbeatres)) {
                fprintf(stderr, "Completed %ld requests\n", done);
                fflush(stderr);
            }
        }
    }

    c->state = STATE_UNCONNECTED;
    io.close(c->aprsock);

    return;
}





void kek(connection_t *c) {
    if (c->keepalive && (c->bread >= c->length)) {
        /* finished a keep-alive connection */
        good++;
        /* save out time */
        if (good == 1) {
            /* first time here */
            doclen = c->bread;
        }
        else if (c->bread != doclen) {
            bad++;
            err_length++;
        }
        if (done < requests) {
            data_t *s = &stats[done++];
            doneka++;
            c->done      = time.now();
            s->starttime = c->start;
            s->ctime     = max(0, time.sub(c->connect, c->start));
            s->time      = max(0, time.sub(c->done, c->start));
            s->waittime  = max(0, time.sub(c->beginread, c->endwrite));
            if (heartbeatres && !(done % heartbeatres)) {
                fprintf(stderr, "Completed %ld requests\n", done);
                fflush(stderr);
            }
        }
        c->keepalive = 0;
        c->length = 0;
        c->gotheader = 0;
        c->cbx = 0;
        c->read = c->bread = 0;
        /* zero connect time with keep-alive */
        c->start = c->connect = lasttime = time.now();
    }
}


/* simple little function to write an APR error string and exit */

void fatal(char *s) {
    fprintf(stderr, "%s: %s (%d)\n", s, strerror(errno), errno);
    if (done) {
        printf("Total of %zu requests completed\n" , done);
    }
    exit(1);
}



int compradre(const void *x, *y) {
    const data_t *a = x;
    const data_t *b = y;
    if ((a->ctime) < (b->ctime)) return -1;
    if ((a->ctime) > (b->ctime)) return 1;
    return 0;
}

int comprando(const void *x, *y) {
    const data_t *a = x;
    const data_t *b = y;
    if ((a->time) < (b->time)) return -1;
    if ((a->time) > (b->time)) return 1;
    return 0;
}

int compri(const void *x, *y) {
    const data_t *a = x;
    const data_t *b = y;
    size_t p = a->time - a->ctime;
    size_t q = b->time - b->ctime;
    if (p < q) return -1;
    if (p > q) return 1;
    return 0;
}

int compwait(const void *x, *y) {
    const data_t *a = x;
    const data_t *b = y;
    if ((a->waittime) < (b->waittime)) return -1;
    if ((a->waittime) > (b->waittime)) return 1;
    return 0;
}

void output_results(int sig) {
    if (sig) {
        lasttime = time.now();  /* record final time if interrupted */
    }
    int64_t timetaken = time.sub(lasttime, start) / time.SECONDS;

    printf("\n\n");
    printf("Server Software:        %s\n", servername);
    printf("Server Hostname:        %s\n", hostname);
    printf("Server Port:            %hu\n", port);
    printf("\n");
    printf("Document Path:          %s\n", path);
    printf("Document Length:        %zu bytes\n", doclen);
    printf("\n");
    printf("Concurrency Level:      %zu\n", concurrency);
    printf("Time taken for tests:   %.3ld seconds\n", timetaken);
    printf("Complete requests:      %ld\n", done);
    printf("Failed requests:        %d\n", bad);
    if (bad)
        printf("   (Connect: %d, Receive: %d, Length: %d, Exceptions: %d)\n",
            err_conn, err_recv, err_length, err_except);
    printf("Write errors:           %d\n", epipe);
    if (err_response)
        printf("Non-2xx responses:      %d\n", err_response);
    if (keepalive)
        printf("Keep-Alive requests:    %d\n", doneka);
    printf("Total transferred:      %ld bytes\n", totalread);
    if (posting > 0)
        printf("Total POSTed:           %ld\n", totalposted);
    printf("HTML transferred:       %ld bytes\n", totalbread);

    /* avoid divide by zero */
    if (timetaken && done) {
        printf("Requests per second:    %.2f [#/sec] (mean)\n",
               (double) done / timetaken);
        printf("Time per request:       %.3f [ms] (mean)\n",
               (double) concurrency * timetaken * 1000 / done);
        printf("Time per request:       %.3f [ms] (mean, across all concurrent requests)\n",
               (double) timetaken * 1000 / done);
        printf("Transfer rate:          %.2f [Kbytes/sec] received\n",
               (double) totalread / 1024 / timetaken);
        if (posting > 0) {
            printf("                        %.2f kb/s sent\n",
               (double) totalposted / timetaken / 1024);
            printf("                        %.2f kb/s total\n",
               (double) (totalread + totalposted) / timetaken / 1024);
        }
    }

    if (done > 0) {
        /* work out connection times */
        int64_t totalcon = 0;
        int64_t total = 0;
        int64_t totald = 0;
        int64_t totalwait = 0;
        int64_t meancon = 0;
        int64_t meantot = 0;
        int64_t meand = 0;
        int64_t meanwait = 0;
        int mincon = INT_MAX;
        int mintot = INT_MAX;
        int mind = INT_MAX;
        int minwait = INT_MAX;
        int maxcon = 0;
        int maxtot = 0;
        int maxd = 0;
        int maxwait = 0;
        int mediancon = 0;
        int mediantot = 0;
        int mediand = 0;
        int medianwait = 0;
        double sdtot = 0;
        double sdcon = 0;
        double sdd = 0;
        double sdwait = 0;

        for (size_t i = 0; i < done; i++) {
            data_t *s = &stats[i];
            mincon = min(mincon, s->ctime);
            mintot = min(mintot, s->time);
            mind = min(mind, s->time - s->ctime);
            minwait = min(minwait, s->waittime);

            maxcon = max(maxcon, s->ctime);
            maxtot = max(maxtot, s->time);
            maxd = max(maxd, s->time - s->ctime);
            maxwait = max(maxwait, s->waittime);

            totalcon += s->ctime;
            total += s->time;
            totald += s->time - s->ctime;
            totalwait += s->waittime;
        }
        meancon = totalcon / done;
        meantot = total / done;
        meand = totald / done;
        meanwait = totalwait / done;

        /* calculating the sample variance: the sum of the squared deviations, divided by n-1 */
        for (size_t i = 0; i < done; i++) {
            data_t *s = &stats[i];
            double a;
            a = ((double)s->time - meantot);
            sdtot += a * a;
            a = ((double)s->ctime - meancon);
            sdcon += a * a;
            a = ((double)s->time - (double)s->ctime - meand);
            sdd += a * a;
            a = ((double)s->waittime - meanwait);
            sdwait += a * a;
        }


        if (done > 1) {
            sdtot = sqrt(sdtot / (done - 1));
            sdcon = sqrt(sdcon / (done - 1));
            sdd = sqrt(sdd / (done - 1));
            sdwait = sqrt(sdwait / (done - 1));
        } else {
            sdtot = 0;
            sdcon = 0;
            sdd = 0;
            sdwait = 0;
        }

        qsort(stats, done, sizeof(data_t), compradre);
        if ((done > 1) && (done % 2)) {
            mediancon = (stats[done / 2].ctime + stats[done / 2 + 1].ctime) / 2;
        } else {
            mediancon = stats[done / 2].ctime;
        }

        qsort(stats, done, sizeof(data_t), compri);
        if ((done > 1) && (done % 2)) {
            mediand = (
                stats[done / 2].time
                + stats[done / 2 + 1].time
                - stats[done / 2].ctime
                - stats[done / 2 + 1].ctime) / 2;
        }
        else {
            mediand = stats[done / 2].time - stats[done / 2].ctime;
        }

        qsort(stats, done, sizeof(data_t), compwait);
        if ((done > 1) && (done % 2))
            medianwait = (stats[done / 2].waittime + stats[done / 2 + 1].waittime) / 2;
        else
            medianwait = stats[done / 2].waittime;

        qsort(stats, done, sizeof(data_t), comprando);
        if ((done > 1) && (done % 2))
            mediantot = (stats[done / 2].time + stats[done / 2 + 1].time) / 2;
        else
            mediantot = stats[done / 2].time;

        printf("\nConnection Times (ms)\n");
        /*
         * Reduce stats from apr time to milliseconds
         */
        mincon     = mincon / time.MS;
        mind       = mind / time.MS;
        minwait    = minwait / time.MS;
        mintot     = mintot / time.MS;
        meancon    = meancon / time.MS;
        meand      = meand / time.MS;
        meanwait   = meanwait / time.MS;
        meantot    = meantot / time.MS;
        mediancon  = mediancon / time.MS;
        mediand    = mediand / time.MS;
        medianwait = medianwait / time.MS;
        mediantot  = mediantot / time.MS;
        maxcon     = maxcon / time.MS;
        maxd       = maxd / time.MS;
        maxwait    = maxwait / time.MS;
        maxtot     = maxtot / time.MS;
        sdcon      = ap_double_ms(sdcon);
        sdd        = ap_double_ms(sdd);
        sdwait     = ap_double_ms(sdwait);
        sdtot      = ap_double_ms(sdtot);

        if (confidence) {
            printf("              min  mean[+/-sd] median   max\n");
            printf("Connect:    %5u %4zu %5.1f %6u %7u\n",
                   mincon, meancon, sdcon, mediancon, maxcon);
            printf("Processing: %5u %4zu %5.1f %6u %7u\n",
                   mind, meand, sdd, mediand, maxd);
            printf("Waiting:    %5u %4zu %5.1f %6u %7u\n",
                   minwait, meanwait, sdwait, medianwait, maxwait);
            printf("Total:      %5u %4zu %5.1f %6u %7u\n",
                   mintot, meantot, sdtot, mediantot, maxtot);

            SANE("the initial connection time", meancon, mediancon, sdcon);
            SANE("the processing time", meand, mediand, sdd);
            SANE("the waiting time", meanwait, medianwait, sdwait);
            SANE("the total time", meantot, mediantot, sdtot);
        }
        else {
            printf("              min   avg   max\n");
            printf("Connect:    %5u %5zu %5u\n", mincon, meancon, maxcon);
            printf("Processing: %5u %5zu %5u\n", mintot - mincon,
                                                   meantot - meancon,
                                                   maxtot - maxcon);
            printf("Total:      %5u %5zu %5u\n", mintot, meantot, maxtot);
        }


        /* Sorted on total connect times */
        if (percentile && (done > 1)) {
            printf("\nPercentage of the requests served within a certain time (ms)\n");
            for (size_t i = 0; i < sizeof(percs) / sizeof(int); i++) {
                if (percs[i] <= 0) {
                    printf(" 0%%  <0> (never)\n");
                } else if (percs[i] >= 100) {
                    printf(" 100%%  %5zu (longest request)\n", stats[done - 1].time);
                } else {
                    printf("  %d%%  %5zu\n", percs[i], stats[(int) (done * percs[i] / 100)].time);
                }
            }
        }
        if (csvperc) {
            FILE *out = fopen(csvperc, "w");
            if (!out) {
                fprintf(stderr, "Cannot open CSV output file: %s\n", strerror(errno));
                exit(1);
            }
            fprintf(out, "" "Percentage served" "," "Time in ms" "\n");
            for (int i = 0; i < 100; i++) {
                double t;
                if (i == 0)
                    t = ap_double_ms(stats[0].time);
                else if (i == 100)
                    t = ap_double_ms(stats[done - 1].time);
                else
                    t = ap_double_ms(stats[(int) (0.5 + done * i / 100.0)].time);
                fprintf(out, "%d,%.3f\n", i, t);
            }
            fclose(out);
        }
        if (gnuplot) {
            FILE *out = fopen(gnuplot, "w");
            if (!out) {
                fprintf(stderr, "Cannot open gnuplot output file: %s", strerror(errno));
                exit(1);
            }
            fprintf(out, "starttime\tseconds\tctime\tdtime\tttime\twait\n");
            for (size_t i = 0; i < done; i++) {
                char tmstring[100] = {0};
                time.format(stats[i].starttime, "%+", tmstring, sizeof(tmstring));
                fprintf(out, "%s\t%d\t%zu\t%zu\t%zu\t%zu\n", tmstring,
                        time.ms(stats[i].starttime) / 1000,
                        stats[i].ctime,
                        stats[i].time - stats[i].ctime,
                        stats[i].time,
                        stats[i].waittime);
            }
            fclose(out);
        }
    }

    if (sig) {
        exit(1);
    }
}

void SANE(const char *what, double mean, median, sd) {
    double d = mean - median;
    if (d < 0) {
        d = -d;
    }
    if (d > 2 * sd ) {
        printf("ERROR: The median and mean for %s are more than twice the standard\n"
            "       deviation apart. These results are not reliable.\n", what);
    }
    else if (d > sd) {
        printf("WARNING: The median and mean for %s are not within a normal deviation\n"
                "        These results are probably not that reliable.\n", what);
    }
}



void err(char *s) {
    fprintf(stderr, "%s\n", s);
    if (done) {
        printf("Total of %ld requests completed\n" , done);
    }
    exit(1);
}

size_t apr_base64_encode_len() {
    panic("todo");
}

int apr_base64_encode() {
    panic("todo");
}