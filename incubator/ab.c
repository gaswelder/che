#import opt
#import strings

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
    AB_MAX = LLONG_MAX
};

/* maximum number of requests on a time limited test */
#define MAX_REQUESTS (INT_MAX > 50000 ? 50000 : INT_MAX)

typedef {
    int TODO;
} apr_time_t;

/* connection state
 * don't add enums or rearrange or otherwise change values without
 * visiting set_conn_state()
 */
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
    apr_pool_t *ctx;
    apr_socket_t *aprsock;
    apr_pollfd_t pollfd;
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
    apr_time_t start,           /* Start of connection */
               connect,         /* Connected, start writing */
               endwrite,        /* Request written */
               beginread,       /* First byte of input */
               done;            /* Connection closed */

    int socknum;
} connection_t;

typedef {
    apr_time_t starttime;         /* start time of connection */
    int waittime; /* between request and reading response */
    int ctime;    /* time to connect */
    int time;     /* time for connection */
} data_t;

#define ap_min(a,b) ((a)<(b))?(a):(b)
#define ap_max(a,b) ((a)>(b))?(a):(b)
#define ap_round_ms(a) ((apr_time_t)((a) + 500)/1000)
#define ap_double_ms(a) ((double)(a)/1000.0)
#define MAX_CONCURRENCY 20000

/* --------------------- GLOBALS ---------------------------- */

int verbosity = 0;      /* no verbosity by default */
int recverrok = 0;      /* ok to proceed after socket receive errors */
int posting = 0;        /* GET by default */
int requests = 1;       /* Number of requests to make */
int heartbeatres = 100; /* How often do we say we're alive */
int concurrency = 1;    /* Number of multiple requests to make */
int percentile = 1;     /* Show percentile served */
int confidence = 1;     /* Show confidence estimator and warnings */
int tlimit = 0;         /* time limit in secs */
bool keepalive = false;      /* try and do keepalive connections */
int windowsize = 0;     /* we use the OS default window size */
char servername[1024] = {0};  /* name that server reports */
char *hostname = NULL;         /* host name from URL */
char *host_field = NULL;       /* value of "Host:" header field */
char *path = NULL;             /* path name */
char postfile[1024] = {0};    /* name of file containing post data */
char *postdata = NULL;         /* *buffer containing data from postfile */
size_t postlen = 0; /* length of data to be POSTed */
char content_type[1024] = {0};/* content type to put in POST header */
char *cookie = NULL;           /* optional cookie line */
char *auth = NULL;             /* optional (basic/uuencoded) auhentication */
char *hdrs = NULL;             /* optional arbitrary headers */
apr_port_t port = 0;        /* port number */
char proxyhost[1024] = {0};   /* proxy host name */
int proxyport = 0;      /* proxy port */
char *connecthost = NULL;
apr_port_t connectport = 0;
char *gnuplot = NULL;          /* GNUplot file */
char *csvperc = NULL;          /* CSV Percentile file */
char url[1024] = {0};
char *fullurl = NULL;
char * colonhost = NULL;
int isproxy = 0;
int aprtimeout = apr_time_from_sec(30); /* timeout value */

/* overrides for ab-generated common headers */
int opt_host = 0;       /* was an optional "Host:" header specified? */
int opt_useragent = 0;  /* was an optional "User-Agent:" header specified? */
int opt_accept = 0;     /* was an optional "Accept:" header specified? */
 /*
  * XXX - this is now a per read/write transact type of value
  */

int use_html = 0;       /* use html in the report */
const char *tablestring = NULL;
const char *trstring = NULL;
const char *tdstring = NULL;

size_t doclen = 0;     /* the length the document should be */
apr_int64_t totalread = 0;    /* total number of bytes read */
apr_int64_t totalbread = 0;   /* totoal amount of entity body read */
apr_int64_t totalposted = 0;  /* total number of bytes posted, inc. headers */
int started = 0;           /* number of requests started, so no excess */
int done = 0;              /* number of requests we have done */
int doneka = 0;            /* number of keep alive connections done */
int good = 0;
int bad = 0;     /* number of good and bad requests */
int epipe = 0;             /* number of broken pipe writes */
int err_length = 0;        /* requests failed due to response length */
int err_conn = 0;          /* requests failed due to connection drop */
int err_recv = 0;          /* requests failed due to broken read */
int err_except = 0;        /* requests failed due to exception */
int err_response = 0;      /* requests with invalid or non-200 response */


apr_time_t start = 0;
apr_time_t lasttime = 0;
apr_time_t stoptime = 0;

/* global request (and its length) */
char _request[2048] = {0};
char *request = _request;
size_t reqlen = 0;

/* one global throw-away buffer to read stuff into */
char buffer[8192] = {0};

/* interesting percentiles */
int percs[] = {50, 66, 75, 80, 90, 95, 98, 99, 100};

connection_t *con = NULL;     /* connection array */
data_t *stats = NULL;         /* data for each request */
apr_pool_t *cntxt = NULL;

apr_pollset_t *readbits = NULL;

apr_sockaddr_t *destsa = NULL;


/* simple little function to write an error string and exit */

void err(char *s)
{
    fprintf(stderr, "%s\n", s);
    if (done)
        printf("Total of %d requests completed\n" , done);
    exit(1);
}

/* simple little function to write an APR error string and exit */

void apr_err(char *s, int rv) {
    char buf[120] = {0};

    fprintf(stderr,
        "%s: %s (%d)\n",
        s, apr_strerror(rv, buf, sizeof(buf)), rv);
    if (done) {
        printf("Total of %d requests completed\n" , done);
    }
    exit(rv);
}

void set_polled_events(connection_t *c, int16_t new_reqevents)
{
    int rv;

    if (c->pollfd.reqevents != new_reqevents) {
        if (c->pollfd.reqevents != 0) {
            if (apr_pollset_remove(readbits, &c->pollfd) != APR_SUCCESS) {
                apr_err("apr_pollset_remove()", rv);
            }
        }

        if (new_reqevents != 0) {
            c->pollfd.reqevents = new_reqevents;
            if (apr_pollset_add(readbits, &c->pollfd) != APR_SUCCESS) {
                apr_err("apr_pollset_add()", rv);
            }
        }
    }
}

void set_conn_state(connection_t *c, int new_state)
{
    int16_t events_by_state[] = {
        0,           /* for STATE_UNCONNECTED */
        APR_POLLOUT, /* for STATE_CONNECTING */
        APR_POLLIN,  /* for STATE_CONNECTED; we don't poll in this state,
                      * so prepare for polling in the following state --
                      * STATE_READ
                      */
        APR_POLLIN   /* for STATE_READ */
    };

    c->state = new_state;

    set_polled_events(c, events_by_state[new_state]);
}

/* --------------------------------------------------------- */
/* write out request to a connection - assumes we can write
 * (small) request out in one go into our new socket buffer
 *
 */

void write_request(connection_t * c)
{
    while (true) {
        apr_time_t tnow;
        size_t l = c->rwrite;
        int e = APR_SUCCESS; /* prevent gcc warning */

        tnow = lasttime = apr_time_now();

        /*
         * First time round ?
         */
        if (c->rwrite == 0) {
            apr_socket_timeout_set(c->aprsock, 0);
            c->connect = tnow;
            c->rwrote = 0;
            c->rwrite = reqlen;
            if (posting)
                c->rwrite += postlen;
        }
        else if (tnow > c->connect + aprtimeout) {
            printf("Send request timed out!\n");
            close_connection(c);
            return;
        }

            e = apr_socket_send(c->aprsock, request + c->rwrote, &l);

        if (e != APR_SUCCESS && !APR_STATUS_IS_EAGAIN(e)) {
            epipe++;
            printf("Send request failed!\n");
            close_connection(c);
            return;
        }
        totalposted += l;
        c->rwrote += l;
        c->rwrite -= l;
        if (!c->rwrite) {
            break;
        }
    }

    c->endwrite = lasttime = apr_time_now();
    set_conn_state(c, STATE_READ);
}

/* --------------------------------------------------------- */

/* calculate and output results */

int compradre(data_t * a, data_t * b)
{
    if ((a->ctime) < (b->ctime))
        return -1;
    if ((a->ctime) > (b->ctime))
        return 1;
    return 0;
}

int comprando(data_t * a, data_t * b)
{
    if ((a->time) < (b->time))
        return -1;
    if ((a->time) > (b->time))
        return 1;
    return 0;
}

int compri(data_t * a, data_t * b)
{
    size_t p = a->time - a->ctime;
    size_t q = b->time - b->ctime;
    if (p < q)
        return -1;
    if (p > q)
        return 1;
    return 0;
}

int compwait(data_t * a, data_t * b)
{
    if ((a->waittime) < (b->waittime))
        return -1;
    if ((a->waittime) > (b->waittime))
        return 1;
    return 0;
}

void output_results(int sig)
{
    double timetaken;

    if (sig) {
        lasttime = apr_time_now();  /* record final time if interrupted */
    }
    timetaken = (double) (lasttime - start) / APR_USEC_PER_SEC;

    printf("\n\n");
    printf("Server Software:        %s\n", servername);
    printf("Server Hostname:        %s\n", hostname);
    printf("Server Port:            %hu\n", port);
    printf("\n");
    printf("Document Path:          %s\n", path);
    printf("Document Length:        %zu bytes\n", doclen);
    printf("\n");
    printf("Concurrency Level:      %d\n", concurrency);
    printf("Time taken for tests:   %.3f seconds\n", timetaken);
    printf("Complete requests:      %d\n", done);
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
        int i;
        apr_time_t totalcon = 0;
        apr_time_t total = 0;
        apr_time_t totald = 0;
        apr_time_t totalwait = 0;
        apr_time_t meancon = 0;
        apr_time_t meantot = 0;
        apr_time_t meand = 0;
        apr_time_t meanwait = 0;
        int mincon = AB_MAX;
        int mintot = AB_MAX;
        int mind = AB_MAX;
        int minwait = AB_MAX;
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

        for (int i = 0; i < done; i++) {
            data_t *s = &stats[i];
            mincon = ap_min(mincon, s->ctime);
            mintot = ap_min(mintot, s->time);
            mind = ap_min(mind, s->time - s->ctime);
            minwait = ap_min(minwait, s->waittime);

            maxcon = ap_max(maxcon, s->ctime);
            maxtot = ap_max(maxtot, s->time);
            maxd = ap_max(maxd, s->time - s->ctime);
            maxwait = ap_max(maxwait, s->waittime);

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
        for (i = 0; i < done; i++) {
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
        if ((done > 1) && (done % 2))
            mediancon = (stats[done / 2].ctime + stats[done / 2 + 1].ctime) / 2;
        else
            mediancon = stats[done / 2].ctime;

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
        mincon     = ap_round_ms(mincon);
        mind       = ap_round_ms(mind);
        minwait    = ap_round_ms(minwait);
        mintot     = ap_round_ms(mintot);
        meancon    = ap_round_ms(meancon);
        meand      = ap_round_ms(meand);
        meanwait   = ap_round_ms(meanwait);
        meantot    = ap_round_ms(meantot);
        mediancon  = ap_round_ms(mediancon);
        mediand    = ap_round_ms(mediand);
        medianwait = ap_round_ms(medianwait);
        mediantot  = ap_round_ms(mediantot);
        maxcon     = ap_round_ms(maxcon);
        maxd       = ap_round_ms(maxd);
        maxwait    = ap_round_ms(maxwait);
        maxtot     = ap_round_ms(maxtot);
        sdcon      = ap_double_ms(sdcon);
        sdd        = ap_double_ms(sdd);
        sdwait     = ap_double_ms(sdwait);
        sdtot      = ap_double_ms(sdtot);

        if (confidence) {
            printf("              min  mean[+/-sd] median   max\n");
            printf("Connect:    %5zu %4zu %5.1f %6zu %7zu\n",
                   mincon, meancon, sdcon, mediancon, maxcon);
            printf("Processing: %5zu %4zu %5.1f %6zu %7zu\n",
                   mind, meand, sdd, mediand, maxd);
            printf("Waiting:    %5zu %4zu %5.1f %6zu %7zu\n",
                   minwait, meanwait, sdwait, medianwait, maxwait);
            printf("Total:      %5zu %4zu %5.1f %6zu %7zu\n",
                   mintot, meantot, sdtot, mediantot, maxtot);

            SANE("the initial connection time", meancon, mediancon, sdcon);
            SANE("the processing time", meand, mediand, sdd);
            SANE("the waiting time", meanwait, medianwait, sdwait);
            SANE("the total time", meantot, mediantot, sdtot);
        }
        else {
            printf("              min   avg   max\n");
            printf("Connect:    %5zu %5zu %5zu\n", mincon, meancon, maxcon);
            printf("Processing: %5zu %5zu %5zu\n", mintot - mincon,
                                                   meantot - meancon,
                                                   maxtot - maxcon);
            printf("Total:      %5zu %5zu %5zu\n", mintot, meantot, maxtot);
        }


        /* Sorted on total connect times */
        if (percentile && (done > 1)) {
            printf("\nPercentage of the requests served within a certain time (ms)\n");
            for (i = 0; i < sizeof(percs) / sizeof(int); i++) {
                if (percs[i] <= 0)
                    printf(" 0%%  <0> (never)\n");
                else if (percs[i] >= 100)
                    printf(" 100%%  %5zu (longest request)\n",
                           ap_round_ms(stats[done - 1].time));
                else
                    printf("  %d%%  %5zu\n", percs[i],
                           ap_round_ms(stats[(int) (done * percs[i] / 100)].time));
            }
        }
        if (csvperc) {
            FILE *out = fopen(csvperc, "w");
            if (!out) {
                perror("Cannot open CSV output file");
                exit(1);
            }
            fprintf(out, "" "Percentage served" "," "Time in ms" "\n");
            for (i = 0; i < 100; i++) {
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
            char tmstring[APR_CTIME_LEN];
            if (!out) {
                perror("Cannot open gnuplot output file");
                exit(1);
            }
            fprintf(out, "starttime\tseconds\tctime\tdtime\tttime\twait\n");
            for (i = 0; i < done; i++) {
                (void) apr_ctime(tmstring, stats[i].starttime);
                fprintf(out, "%s\t%zu\t%zu\t%zu\t%zu\t%zu\n", tmstring,
                        apr_time_sec(stats[i].starttime),
                        ap_round_ms(stats[i].ctime),
                        ap_round_ms(stats[i].time - stats[i].ctime),
                        ap_round_ms(stats[i].time),
                        ap_round_ms(stats[i].waittime));
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

/* --------------------------------------------------------- */

/* calculate and output results in HTML  */

void output_html_results()
{
    double timetaken = (double) (lasttime - start) / APR_USEC_PER_SEC;

    printf("\n\n<table %s>\n", tablestring);
    printf("<tr %s><th colspan=2 %s>Server Software:</th>"
       "<td colspan=2 %s>%s</td></tr>\n",
       trstring, tdstring, tdstring, servername);
    printf("<tr %s><th colspan=2 %s>Server Hostname:</th>"
       "<td colspan=2 %s>%s</td></tr>\n",
       trstring, tdstring, tdstring, hostname);
    printf("<tr %s><th colspan=2 %s>Server Port:</th>"
       "<td colspan=2 %s>%hu</td></tr>\n",
       trstring, tdstring, tdstring, port);
    printf("<tr %s><th colspan=2 %s>Document Path:</th>"
       "<td colspan=2 %s>%s</td></tr>\n",
       trstring, tdstring, tdstring, path);
    printf("<tr %s><th colspan=2 %s>Document Length:</th>"
       "<td colspan=2 %s>%zu bytes</td></tr>\n",
       trstring, tdstring, tdstring, doclen);
    printf("<tr %s><th colspan=2 %s>Concurrency Level:</th>"
       "<td colspan=2 %s>%d</td></tr>\n",
       trstring, tdstring, tdstring, concurrency);
    printf("<tr %s><th colspan=2 %s>Time taken for tests:</th>"
       "<td colspan=2 %s>%.3f seconds</td></tr>\n",
       trstring, tdstring, tdstring, timetaken);
    printf("<tr %s><th colspan=2 %s>Complete requests:</th>"
       "<td colspan=2 %s>%d</td></tr>\n",
       trstring, tdstring, tdstring, done);
    printf("<tr %s><th colspan=2 %s>Failed requests:</th>"
       "<td colspan=2 %s>%d</td></tr>\n",
       trstring, tdstring, tdstring, bad);
    if (bad)
        printf("<tr %s><td colspan=4 %s >   (Connect: %d, Length: %d, Exceptions: %d)</td></tr>\n",
           trstring, tdstring, err_conn, err_length, err_except);
    if (err_response)
        printf("<tr %s><th colspan=2 %s>Non-2xx responses:</th>"
           "<td colspan=2 %s>%d</td></tr>\n",
           trstring, tdstring, tdstring, err_response);
    if (keepalive)
        printf("<tr %s><th colspan=2 %s>Keep-Alive requests:</th>"
           "<td colspan=2 %s>%d</td></tr>\n",
           trstring, tdstring, tdstring, doneka);
    printf("<tr %s><th colspan=2 %s>Total transferred:</th>"
       "<td colspan=2 %s>%ld bytes</td></tr>\n",
       trstring, tdstring, tdstring, totalread);
    if (posting > 0)
        printf("<tr %s><th colspan=2 %s>Total POSTed:</th>"
           "<td colspan=2 %s>%ld</td></tr>\n",
           trstring, tdstring, tdstring, totalposted);
    printf("<tr %s><th colspan=2 %s>HTML transferred:</th>"
       "<td colspan=2 %s>%ld bytes</td></tr>\n",
       trstring, tdstring, tdstring, totalbread);

    /* avoid divide by zero */
    if (timetaken) {
        printf("<tr %s><th colspan=2 %s>Requests per second:</th>"
           "<td colspan=2 %s>%.2f</td></tr>\n",
           trstring, tdstring, tdstring, (double) done * 1000 / timetaken);
        printf("<tr %s><th colspan=2 %s>Transfer rate:</th>"
           "<td colspan=2 %s>%.2f kb/s received</td></tr>\n",
           trstring, tdstring, tdstring, (double) totalread / timetaken);
        if (posting > 0) {
            printf("<tr %s><td colspan=2 %s>&nbsp;</td>"
               "<td colspan=2 %s>%.2f kb/s sent</td></tr>\n",
               trstring, tdstring, tdstring,
               (double) totalposted / timetaken);
            printf("<tr %s><td colspan=2 %s>&nbsp;</td>"
               "<td colspan=2 %s>%.2f kb/s total</td></tr>\n",
               trstring, tdstring, tdstring,
               (double) (totalread + totalposted) / timetaken);
        }
    }
    /* work out connection times */
    int i = 0;
    int totalcon = 0;
    int total = 0;
    int mincon = AB_MAX;
    int mintot = AB_MAX;
    int maxcon = 0;
    int maxtot = 0;

    for (int i = 0; i < done; i++) {
        data_t *s = &stats[i];
        mincon = ap_min(mincon, s->ctime);
        mintot = ap_min(mintot, s->time);
        maxcon = ap_max(maxcon, s->ctime);
        maxtot = ap_max(maxtot, s->time);
        totalcon += s->ctime;
        total    += s->time;
    }
    /*
        * Reduce stats from apr time to milliseconds
        */
    mincon   = ap_round_ms(mincon);
    mintot   = ap_round_ms(mintot);
    maxcon   = ap_round_ms(maxcon);
    maxtot   = ap_round_ms(maxtot);
    totalcon = ap_round_ms(totalcon);
    total    = ap_round_ms(total);

    if (done > 0) { /* avoid division by zero (if 0 done) */
        printf("<tr %s><th %s colspan=4>Connnection Times (ms)</th></tr>\n",
            trstring, tdstring);
        printf("<tr %s><th %s>&nbsp;</th> <th %s>min</th>   <th %s>avg</th>   <th %s>max</th></tr>\n",
            trstring, tdstring, tdstring, tdstring, tdstring);
        printf("<tr %s><th %s>Connect:</th>"
            "<td %s>%5zu</td>"
            "<td %s>%5zu</td>"
            "<td %s>%5zu</td></tr>\n",
            trstring, tdstring, tdstring, mincon, tdstring, totalcon / done, tdstring, maxcon);
        printf("<tr %s><th %s>Processing:</th>"
            "<td %s>%5zu</td>"
            "<td %s>%5zu</td>"
            "<td %s>%5zu</td></tr>\n",
            trstring, tdstring, tdstring, mintot - mincon, tdstring,
            (total / done) - (totalcon / done), tdstring, maxtot - maxcon);
        printf("<tr %s><th %s>Total:</th>"
            "<td %s>%5zu</td>"
            "<td %s>%5zu</td>"
            "<td %s>%5zu</td></tr>\n",
            trstring, tdstring, tdstring, mintot, tdstring, total / done, tdstring, maxtot);
    }
    printf("</table>\n");
}

/* --------------------------------------------------------- */

/* start asnchronous non-blocking connection */

void start_connect(connection_t * c)
{
    int rv;

    if (!(started < requests))
    return;

    c->read = 0;
    c->bread = 0;
    c->keepalive = 0;
    c->cbx = 0;
    c->gotheader = 0;
    c->rwrite = 0;
    if (c->ctx)
        apr_pool_clear(c->ctx);
    else
        apr_pool_create(&c->ctx, cntxt);

    if ((rv = apr_socket_create(&c->aprsock, destsa->family,
                SOCK_STREAM, 0, c->ctx)) != APR_SUCCESS) {
    apr_err("socket", rv);
    }

    c->pollfd.desc_type = APR_POLL_SOCKET;
    c->pollfd.desc.s = c->aprsock;
    c->pollfd.reqevents = 0;
    c->pollfd.client_data = c;

    if ((rv = apr_socket_opt_set(c->aprsock, APR_SO_NONBLOCK, 1))
         != APR_SUCCESS) {
        apr_err("socket nonblock", rv);
    }

    if (windowsize != 0) {
        rv = apr_socket_opt_set(c->aprsock, APR_SO_SNDBUF, 
                                windowsize);
        if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
            apr_err("socket send buffer", rv);
        }
        rv = apr_socket_opt_set(c->aprsock, APR_SO_RCVBUF, 
                                windowsize);
        if (rv != APR_SUCCESS && rv != APR_ENOTIMPL) {
            apr_err("socket receive buffer", rv);
        }
    }

    c->start = lasttime = apr_time_now();
    if ((rv = apr_socket_connect(c->aprsock, destsa)) != APR_SUCCESS) {
        if (APR_STATUS_IS_EINPROGRESS(rv)) {
            set_conn_state(c, STATE_CONNECTING);
            c->rwrite = 0;
            return;
        }
        else {
            set_conn_state(c, STATE_UNCONNECTED);
            apr_socket_close(c->aprsock);
            err_conn++;
            if (bad++ > 10) {
                fprintf(stderr,
                   "\nTest aborted after 10 failures\n\n");
                apr_err("apr_socket_connect()", rv);
            }
            
            start_connect(c);
            return;
        }
    }

    /* connected first time */
    set_conn_state(c, STATE_CONNECTED);
    started++;
    write_request(c);
}

/* --------------------------------------------------------- */

/* close down connection and save stats */

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
            c->done      = lasttime = apr_time_now();
            s->starttime = c->start;
            s->ctime     = ap_max(0, c->connect - c->start);
            s->time      = ap_max(0, c->done - c->start);
            s->waittime  = ap_max(0, c->beginread - c->endwrite);
            if (heartbeatres && !(done % heartbeatres)) {
                fprintf(stderr, "Completed %d requests\n", done);
                fflush(stderr);
            }
        }
    }

    set_conn_state(c, STATE_UNCONNECTED);
    apr_socket_close(c->aprsock);

    /* connect again */
    start_connect(c);
    return;
}

/* --------------------------------------------------------- */

/* read data from connection */

void read_connection(connection_t * c)
{
    size_t r;
    int status;
    char *part;
    char respcode[4];       /* 3 digits and null */

    r = sizeof(buffer);
    status = apr_socket_recv(c->aprsock, buffer, &r);
    if (APR_STATUS_IS_EAGAIN(status))
        return;

    if (r == 0 && APR_STATUS_IS_EOF(status)) {
        good++;
        close_connection(c);
        return;
    }
    /* catch legitimate fatal apr_socket_recv errors */
    if (status != APR_SUCCESS) {
        err_recv++;
        if (recverrok) {
            bad++;
            close_connection(c);
            if (verbosity >= 1) {
                char buf[120];
                fprintf(stderr,"%s: %s (%d)\n", "apr_socket_recv", apr_strerror(status, buf, sizeof(buf)), status);
            }
            return;
        } else {
            apr_err("apr_socket_recv", status);
        }
    }

    totalread += r;
    if (c->read == 0) {
        c->beginread = apr_time_now();
    }
    c->read += r;


    if (!c->gotheader) {
        char *s;
        int l = 4;
        size_t space = CBUFFSIZE - c->cbx - 1; /* -1 allows for \0 term */
        int tocopy = r;
        if (space < r) {
            tocopy = space;
        }
        memcpy(c->cbuff + c->cbx, buffer, space);
        c->cbx += tocopy;
        space -= tocopy;
        c->cbuff[c->cbx] = 0;   /* terminate for benefit of strstr */
        if (verbosity >= 2) {
            printf("LOG: header received:\n%s\n", c->cbuff);
        }
        s = strstr(c->cbuff, "\r\n\r\n");
        /*
         * this next line is so that we talk to NCSA 1.5 which blatantly
         * breaks the http specifaction
         */
        if (!s) {
            s = strstr(c->cbuff, "\n\n");
            l = 2;
        }

        if (!s) {
            /* read rest next time */
            if (space) {
                return;
            }
            else {
            /* header is in invalid or too big - close connection */
                set_conn_state(c, STATE_UNCONNECTED);
                apr_socket_close(c->aprsock);
                err_response++;
                if (bad++ > 10) {
                    err("\nTest aborted after 10 failures\n\n");
                }
                start_connect(c);
            }
        }
        else {
            /* have full header */
            if (!good) {
                /*
                 * this is first time, extract some interesting info
                 */
                char *p = NULL;
                char *q = NULL;
                p = strstr(c->cbuff, "Server:");
                q = servername;
                if (p) {
                    p += 8;
                    while (*p > 32)
                    *q++ = *p++;
                }
                *q = 0;
            }
            /*
             * XXX: this parsing isn't even remotely HTTP compliant... but in
             * the interest of speed it doesn't totally have to be, it just
             * needs to be extended to handle whatever servers folks want to
             * test against. -djg
             */

            /* check response code */
            part = strstr(c->cbuff, "HTTP");    /* really HTTP/1.x_ */
            if (part && strlen(part) > strlen("HTTP/1.x_")) {
                strncpy(respcode, (part + strlen("HTTP/1.x_")), 3);
                respcode[3] = '\0';
            }
            else {
                strcpy(respcode, "500");
            }

            if (respcode[0] != '2') {
                err_response++;
                if (verbosity >= 2)
                    printf("WARNING: Response code not 2xx (%s)\n", respcode);
            }
            else if (verbosity >= 3) {
                printf("LOG: Response code = %s\n", respcode);
            }
            c->gotheader = 1;
            *s = 0;     /* terminate at end of header */
            if (keepalive &&
            (strstr(c->cbuff, "Keep-Alive")
             || strstr(c->cbuff, "keep-alive"))) {  /* for benefit of MSIIS */
                char *cl;
                cl = strstr(c->cbuff, "Content-Length:");
                /* handle NCSA, which sends Content-length: */
                if (!cl)
                    cl = strstr(c->cbuff, "Content-length:");
                if (cl) {
                    c->keepalive = 1;
                    /* response to HEAD doesn't have entity body */
                    if (posting >= 0) {
                        c->length = atoi(cl + 16);
                    } else {
                        c->length = 0;
                    }
                }
                /* The response may not have a Content-Length header */
                if (!cl) {
                    c->keepalive = 1;
                    c->length = 0; 
                }
            }
            c->bread += c->cbx - (s + l - c->cbuff) + r - tocopy;
            totalbread += c->bread;
        }
    }
    else {
        /* outside header, everything we have read is entity body */
        c->bread += r;
        totalbread += r;
    }

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
            c->done      = apr_time_now();
            s->starttime = c->start;
            s->ctime     = ap_max(0, c->connect - c->start);
            s->time      = ap_max(0, c->done - c->start);
            s->waittime  = ap_max(0, c->beginread - c->endwrite);
            if (heartbeatres && !(done % heartbeatres)) {
                fprintf(stderr, "Completed %d requests\n", done);
                fflush(stderr);
            }
        }
        c->keepalive = 0;
        c->length = 0;
        c->gotheader = 0;
        c->cbx = 0;
        c->read = c->bread = 0;
        /* zero connect time with keep-alive */
        c->start = c->connect = lasttime = apr_time_now();
        write_request(c);
    }
}

/* --------------------------------------------------------- */

/* run the tests */

void test()
{
    apr_time_t stoptime;
    int16_t rv;
    int i;
    int status;
    int snprintf_res = 0;

    if (isproxy) {
        connecthost = apr_pstrdup(cntxt, proxyhost);
        connectport = proxyport;
    }
    else {
        connecthost = apr_pstrdup(cntxt, hostname);
        connectport = port;
    }

    if (!use_html) {
        printf("Benchmarking %s ", hostname);
    if (isproxy)
        printf("[through %s:%d] ", proxyhost, proxyport);
    if (heartbeatres) {
        printf("(be patient)\n");
    } else {
        printf("(be patient)...");
    }
    fflush(stdout);
    }

    con = calloc(concurrency, sizeof(connection_t));

    stats = calloc(requests, sizeof(data_t));

    if ((status = apr_pollset_create(&readbits, concurrency, cntxt,
                                     APR_POLLSET_NOCOPY)) != APR_SUCCESS) {
        apr_err("apr_pollset_create failed", status);
    }

    /* add default headers if necessary */
    if (!opt_host) {
        /* Host: header not overridden, add default value to hdrs */
        hdrs = apr_pstrcat(cntxt, hdrs, "Host: ", host_field, colonhost, "\r\n", NULL);
    }
    else {
        /* Header overridden, no need to add, as it is already in hdrs */
    }

    if (!opt_useragent) {
        /* User-Agent: header not overridden, add default value to hdrs */
        hdrs = apr_pstrcat(cntxt, hdrs, "User-Agent: ApacheBench/", AP_AB_BASEREVISION, "\r\n", NULL);
    }
    else {
        /* Header overridden, no need to add, as it is already in hdrs */
    }

    if (!opt_accept) {
        /* Accept: header not overridden, add default value to hdrs */
        hdrs = apr_pstrcat(cntxt, hdrs, "Accept: */*\r\n", NULL);
    }
    else {
        /* Header overridden, no need to add, as it is already in hdrs */
    }

    /* setup request */
    if (posting <= 0) {
        const char *method = "GET";
        if (posting) {
            method = "HEAD";
        }
        const char *ppath = path;
        if (isproxy) {
            ppath = fullurl;
        }
        const char *kas = "";
        if (keepalive) {
            kas = "Connection: Keep-Alive\r\n";
        }
        snprintf_res = apr_snprintf(request, sizeof(_request),
            "%s %s HTTP/1.0\r\n"
            "%s" "%s" "%s"
            "%s" "\r\n",
            method,
            ppath,
            kas,
            cookie, auth, hdrs);
    }
    else {
        const char *ppath = path;
        if (isproxy) {
            ppath = fullurl;
        }
        const char *ctype = "text/plain";
        if (content_type[0]) {
            ctype = content_type;
        }
        const char *kas = "";
        if (keepalive) {
            kas = "Connection: Keep-Alive\r\n";
        }
        snprintf_res = apr_snprintf(request,  sizeof(_request),
            "POST %s HTTP/1.0\r\n"
            "%s" "%s" "%s"
            "Content-length: %zu\r\n"
            "Content-type: %s\r\n"
            "%s"
            "\r\n",
            ppath,
            kas,
            cookie, auth,
            postlen,
            ctype, hdrs);
    }
    if (snprintf_res >= sizeof(_request)) {
        err("Request too long\n");
    }

    if (verbosity >= 2)
        printf("INFO: POST header == \n---\n%s\n---\n", request);

    reqlen = strlen(request);

    /*
     * Combine headers and (optional) post file into one contineous buffer
     */
    if (posting == 1) {
        char *buff = calloc(postlen + reqlen + 1, 1);
        if (!buff) {
            fprintf(stderr, "error creating request buffer: out of memory\n");
            return;
        }
        strcpy(buff, request);
        memcpy(buff + reqlen, postdata, postlen);
        request = buff;
    }


    /* This only needs to be done once */
    if ((rv = apr_sockaddr_info_get(&destsa, connecthost, APR_UNSPEC, connectport, 0, cntxt))
       != APR_SUCCESS) {
        char buf[120];
        apr_snprintf(buf, sizeof(buf),
                 "apr_sockaddr_info_get() for %s", connecthost);
        apr_err(buf, rv);
    }

    /* ok - lets start */
    start = lasttime = apr_time_now();
    if (tlimit) {
        stoptime = (start + apr_time_from_sec(tlimit));
    } else {
        stoptime = AB_MAX;
    }

    /* Output the results if the user terminates the run early. */
    apr_signal(SIGINT, output_results);

    /* initialise lots of requests */
    for (i = 0; i < concurrency; i++) {
        con[i].socknum = i;
        start_connect(&con[i]);
    }

    while (true) {
        int32_t n;
        const apr_pollfd_t *pollresults;

        n = concurrency;
        while (true) {
            status = apr_pollset_poll(readbits, aprtimeout, &n, &pollresults);
            if (!APR_STATUS_IS_EINTR(status)) {
                break;
            }
        }
        if (status != APR_SUCCESS)
            apr_err("apr_poll", status);

        if (!n) {
            err("\nServer timed out\n\n");
        }

        for (i = 0; i < n; i++) {
            const apr_pollfd_t *next_fd = &(pollresults[i]);
            connection_t *c;

            c = next_fd->client_data;

            /*
             * If the connection isn't connected how can we check it?
             */
            if (c->state == STATE_UNCONNECTED)
                continue;

            rv = next_fd->rtnevents;


            /*
             * Notes: APR_POLLHUP is set after FIN is received on some
             * systems, so treat that like APR_POLLIN so that we try to read
             * again.
             *
             * Some systems return APR_POLLERR with APR_POLLHUP.  We need to
             * call read_connection() for APR_POLLHUP, so check for
             * APR_POLLHUP first so that a closed connection isn't treated
             * like an I/O error.  If it is, we never figure out that the
             * connection is done and we loop here endlessly calling
             * apr_poll().
             */
            if ((rv & APR_POLLIN) || (rv & APR_POLLPRI) || (rv & APR_POLLHUP))
                read_connection(c);
            if ((rv & APR_POLLERR) || (rv & APR_POLLNVAL)) {
                bad++;
                err_except++;
                /* avoid apr_poll/EINPROGRESS loop on HP-UX, let recv discover ECONNREFUSED */
                if (c->state == STATE_CONNECTING) { 
                    read_connection(c);
                }
                else { 
                    start_connect(c);
                }
                continue;
            }
            if (rv & APR_POLLOUT) {
                if (c->state == STATE_CONNECTING) {
                    rv = apr_socket_connect(c->aprsock, destsa);
                    if (rv != APR_SUCCESS) {
                        apr_socket_close(c->aprsock);
                        err_conn++;
                        if (bad++ > 10) {
                            fprintf(stderr,
                                    "\nTest aborted after 10 failures\n\n");
                            apr_err("apr_socket_connect()", rv);
                        }
                        set_conn_state(c, STATE_UNCONNECTED);
                        start_connect(c);
                        continue;
                    }
                    else {
                        set_conn_state(c, STATE_CONNECTED);
                        started++;
                        write_request(c);
                    }
                }
                else {
                    write_request(c);
                }
            }
        }
        if (lasttime < stoptime && done < requests) {
            continue;
        } else {
            break;
        }
    }
    
    if (heartbeatres)
        fprintf(stderr, "Finished %d requests\n", done);
    else
        printf("..done\n");

    if (use_html)
        output_html_results();
    else
        output_results(0);
}

void copyright() {
    if (!use_html) {
        printf("This is ApacheBench, Version %s\n", AP_AB_BASEREVISION);
        printf("Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/\n");
        printf("Licensed to The Apache Software Foundation, http://www.apache.org/\n");
        printf("\n");
    }
    else {
        printf("<p>\n");
        printf(" This is ApacheBench, Version %s <i>&lt;%s&gt;</i><br>\n", AP_AB_BASEREVISION, "$Revision$");
        printf(" Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/<br>\n");
        printf(" Licensed to The Apache Software Foundation, http://www.apache.org/<br>\n");
        printf("</p>\n<p>\n");
    }
}

/* display usage information */
void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [options] [http"
        "://]hostname[:port]/path\n", progname);
/* 80 column ruler:  ********************************************************************************
 */
    fprintf(stderr, "Options are:\n");
    fprintf(stderr, "    -n requests     Number of requests to perform\n");
    fprintf(stderr, "    -c concurrency  Number of multiple requests to make\n");
    fprintf(stderr, "    -t timelimit    Seconds to max. wait for responses\n");
    fprintf(stderr, "    -b windowsize   Size of TCP send/receive buffer, in bytes\n");
    fprintf(stderr, "    -p postfile     File containing data to POST. Remember also to set -T\n");
    fprintf(stderr, "    -T content-type Content-type header for POSTing, eg.\n");
    fprintf(stderr, "                    'application/x-www-form-urlencoded'\n");
    fprintf(stderr, "                    Default is 'text/plain'\n");
    fprintf(stderr, "    -v verbosity    How much troubleshooting info to print\n");
    fprintf(stderr, "    -w              Print out results in HTML tables\n");
    fprintf(stderr, "    -i              Use HEAD instead of GET\n");
    fprintf(stderr, "    -x attributes   String to insert as table attributes\n");
    fprintf(stderr, "    -y attributes   String to insert as tr attributes\n");
    fprintf(stderr, "    -z attributes   String to insert as td or th attributes\n");
    fprintf(stderr, "    -C attribute    Add cookie, eg. 'Apache=1234. (repeatable)\n");
    fprintf(stderr, "    -H attribute    Add Arbitrary header line, eg. 'Accept-Encoding: gzip'\n");
    fprintf(stderr, "                    Inserted after all normal header lines. (repeatable)\n");
    fprintf(stderr, "    -A attribute    Add Basic WWW Authentication, the attributes\n");
    fprintf(stderr, "                    are a colon separated username and password.\n");
    fprintf(stderr, "    -P attribute    Add Basic Proxy Authentication, the attributes\n");
    fprintf(stderr, "                    are a colon separated username and password.\n");
    fprintf(stderr, "    -X proxy:port   Proxyserver and port number to use\n");
    fprintf(stderr, "    -V              Print version number and exit\n");
    fprintf(stderr, "    -k              Use HTTP KeepAlive feature\n");
    fprintf(stderr, "    -d              Do not show percentiles served table.\n");
    fprintf(stderr, "    -S              Do not show confidence estimators and warnings.\n");
    fprintf(stderr, "    -g filename     Output collected data to gnuplot format file.\n");
    fprintf(stderr, "    -e filename     Output CSV file with percentages served\n");
    fprintf(stderr, "    -r              Don't exit on socket receive errors.\n");
    fprintf(stderr, "    -h              Display usage information (this message)\n");
    exit(EINVAL);
}

/* ------------------------------------------------------- */

/* split URL into parts */

int parse_url(char *url)
{
    char *cp;
    char *h;
    char *scope_id;
    int rv;

    /* Save a copy for the proxy */
    fullurl = apr_pstrdup(cntxt, url);

    if (strlen(url) > 7 && strncmp(url, "http://", 7) == 0) {
        url += 7;
    }
    else
    if (strlen(url) > 8 && strncmp(url, "https://", 8) == 0) {
        fprintf(stderr, "SSL not compiled in; no https support\n");
        exit(1);
    }

    if ((cp = strchr(url, '/')) == NULL)
        return 1;
    h = apr_pstrmemdup(cntxt, url, cp - url);
    rv = apr_parse_addr_port(&hostname, &scope_id, &port, h, cntxt);
    if (rv != APR_SUCCESS || !hostname || scope_id) {
        return 1;
    }
    path = apr_pstrdup(cntxt, cp);
    *cp = '\0';
    if (*url == '[') {      /* IPv6 numeric address string */
        host_field = apr_psprintf(cntxt, "[%s]", hostname);
    }
    else {
        host_field = hostname;
    }

    if (port == 0) {        /* no port specified */
        port = 80;
    }

    if ((
         (port != 80)))
    {
        colonhost = apr_psprintf(cntxt,":%d",port);
    } else
        colonhost = "";
    return 0;
}

/* ------------------------------------------------------- */

/* read data to POST from file, save contents and length */

typedef {
    int TODO;
} apr_finfo_t;

int open_postfile(const char *pfile)
{
    apr_file_t *postfd;
    apr_finfo_t finfo;
    int rv;
    char errmsg[120];

    rv = apr_file_open(&postfd, pfile, APR_READ, APR_OS_DEFAULT, cntxt);
    if (rv != APR_SUCCESS) {
        fprintf(stderr, "ab: Could not open POST data file (%s): %s\n", pfile,
                apr_strerror(rv, errmsg, sizeof(errmsg)));
        return rv;
    }

    rv = apr_file_info_get(&finfo, APR_FINFO_NORM, postfd);
    if (rv != APR_SUCCESS) {
        fprintf(stderr, "ab: Could not stat POST data file (%s): %s\n", pfile,
                apr_strerror(rv, errmsg, sizeof(errmsg)));
        return rv;
    }
    postlen = (size_t)finfo.size;
    postdata = malloc(postlen);
    if (!postdata) {
        fprintf(stderr, "ab: Could not allocate POST data buffer\n");
        return APR_ENOMEM;
    }
    rv = apr_file_read_full(postfd, postdata, postlen, NULL);
    if (rv != APR_SUCCESS) {
        fprintf(stderr, "ab: Could not read POST data file: %s\n",
                apr_strerror(rv, errmsg, sizeof(errmsg)));
        return rv;
    }
    apr_file_close(postfd);
    return 0;
}

int main(int argc, char *argv[]) {
    int r;
    int l;
    char tmp[1024];
    int status;
    const char *optarg;
    char c;

    /* table defaults  */
    tablestring = "";
    trstring = "";
    tdstring = "bgcolor=white";
    cookie = "";
    auth = "";
    proxyhost[0] = '\0';
    hdrs = "";

    apr_app_initialize(&argc, &argv, NULL);
    apr_pool_create(&cntxt, NULL);

    bool quiet = false;

    opt.opt_bool("k", "keep-alive", &keepalive);
    opt.opt_bool("q", "quiet", &quiet);
    opt.opt_int("c", "concurrency", &concurrency);
    opt.opt_int("b", "windowsize", &windowsize);
    opt.opt_size("n", "number of requests", &requests);
    opt.opt_bool("w", "use_html", &use_html);
    opt.opt_int("v", "verbosity", &verbosity);
    opt.opt_str("g", "gnuplot output file", &gnuplot);

    bool hflag = false;
    opt.opt_bool("h", "print usage", &hflag);
    bool vflag = false;
    opt.opt_bool("V", "print version", &vflag);

    bool iflag = false;
    opt.opt_bool("i", "use head", &iflag);


    bool dflag = false;
    opt.opt_bool("d", "percentile = 0", &dflag);
    bool sflag = false;
    opt.opt_bool("S", "confidence = 0", &sflag);

    opt.opt_str("e", "csvperc", &csvperc);


    char *postfile = NULL;
    opt.opt_str("p", "postfile", &postfile);

    opt.opt_bool("r", "recverrok = 1", &recverrok);
    opt.opt_size("t", "tlimit", &tlimit);

    
    char *autharg = NULL;
    opt.opt_str("A", "auth", &autharg);

    opt.opt_str("T", "content_type", &content_type);
    char *cookiearg = NULL;
    opt.opt_str("C", "cookie", &cookiearg);

    char *pflag = NULL;
    opt.opt_str("P", "pflag", &pflag);
    char *hflag = NULL;
    opt.opt_str("H", "hflag", &hflag);
    char *xarg = NULL;
    char *Xarg = NULL;
    char *yarg = NULL;
    char *zarg = NULL;
    opt.opt_str("x", "xarg", &xarg);
    opt.opt_str("X", "Xarg", &Xarg);
    opt.opt_str("y", "yarg", &yarg);
    opt.opt_str("z", "zarg", &zarg);
    

    char **args = opt.opt_parse(argc, argv);
    if (!*args) {
        fprintf(stderr, "missing the url argument\n");
        return 1;
    }
    if (parse_url(apr_pstrdup(cntxt, *args))) {
        fprintf(stderr, "%s: invalid URL\n", argv[0]);
        usage(argv[0]);
        return 1;
    }
    args++;
    if (*args) {
        fprintf(stderr, "unexpected argument: %s\n", *args);
        return 1;
    }
    
    if (cookiearg) {
        cookie = apr_pstrcat(cntxt, "Cookie: ", cookiearg, "\r\n", NULL);
    }

    if (autharg) {
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
        auth = apr_pstrcat(cntxt, auth, "Authorization: Basic ", tmp, "\r\n", NULL);
    }
    

    if (tlimit) {
        // need to size data array on something.
        requests = MAX_REQUESTS;
    }
    if (postfile) {
        if (posting != 0) {
            err("Cannot mix POST and HEAD\n");
        }
        int r = open_postfile(postfile);
        if (r == 0) {
            posting = 1;
        } else if (postdata) {
            exit(r);
        }
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
        copyright();
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

    

    if (pflag) {
        char *optarg = pflag;
        //  assume username passwd already to be in colon separated form.
        while (isspace(*optarg)) optarg++;
        if (apr_base64_encode_len(strlen(optarg)) > sizeof(tmp)) {
            err("Proxy credentials too long\n");
        }
        l = apr_base64_encode(tmp, optarg, strlen(optarg));
        tmp[l] = '\0';

        auth = apr_pstrcat(cntxt, auth, "Proxy-Authorization: Basic ",
                                tmp, "\r\n", NULL);
    }
    
    if (hflag) {
        char *optarg = hflag;
        hdrs = apr_pstrcat(cntxt, hdrs, optarg, "\r\n", NULL);
        /*
            * allow override of some of the common headers that ab adds
            */
        if (strings.starts_with_i(optarg, "Host:")) {
            opt_host = 1;
        } else if (strings.starts_with_i(optarg, "Accept:")) {
            opt_accept = 1;
        } else if (strings.starts_with_i(optarg, "User-Agent:")) {
            opt_useragent = 1;
        }
    }
    /*
    * if any of the following three are used, turn on html output
    * automatically
    */
    if (xarg) {
        use_html = 1;
        tablestring = xarg;
    }
    if (Xarg) {
        char *optarg = Xarg;
        char *p;
        /*
            * assume proxy-name[:port]
            */
        if ((p = strchr(optarg, ':'))) {
            *p = '\0';
            p++;
            proxyport = atoi(p);
        }
        strcpy(proxyhost, optarg);
        isproxy = 1;
    }
    
    if (yarg) {
        use_html = 1;
        trstring = yarg;
    }
    if (zarg) {
        use_html = 1;
        tdstring = zarg;
    }


    if ((concurrency < 0) || (concurrency > MAX_CONCURRENCY)) {
        fprintf(stderr, "%s: Invalid Concurrency [Range 0..%d]\n",
                argv[0], MAX_CONCURRENCY);
        usage(argv[0]);
    }

    if (concurrency > requests) {
        fprintf(stderr, "%s: Cannot use concurrency level greater than "
                "total number of requests\n", argv[0]);
        usage(argv[0]);
    }

    if ((heartbeatres) && (requests > 150)) {
        heartbeatres = requests / 10;   /* Print line every 10% of requests */
        if (heartbeatres < 100)
            heartbeatres = 100; /* but never more often than once every 100
                                 * connections. */
    }
    else
        heartbeatres = 0;

    copyright();
    test();
    return 0;
}
