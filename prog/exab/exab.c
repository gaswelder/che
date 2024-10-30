#import dbg
#import fs
#import http
#import ioroutine
#import opt
#import os/io
#import strings
#import time

typedef {
    time.t time_started;
    time.t time_finished_writing;
    time.t time_finished_reading;
	int status;
	size_t total_received;
	size_t total_sent;
	int error;
} result_t;

typedef {
    io.handle_t *connection; // Connection with the server
    io.buf_t *outbuf; // Request copy to send
    io.buf_t *inbuf; // Response to read
    int body_bytes_to_read; // How many bytes to read to get the full response.
    result_t *stats; // Current request's stats
} context_t;

int _bad = 0;
void bad() {
	_bad++;
	if (_bad > 10) {
		fatal("Test aborted after 10 failures");
	}
}

result_t *request_stats = NULL;
size_t requests_done = 0;
char *colonhost = "";
size_t requests_to_do = 1;
io.buf_t REQUEST = {};

int main(int argc, char *argv[]) {
    opt.opt_summary("exab [options] <url> - makes HTTP requests to <url> and prints statistics");

    opt.size("n", "number of requests to perform", &requests_to_do);

	size_t concurrency = 1;
    opt.size("c", "number of requests running concurrently", &concurrency);

    char *methodstring = NULL;
    opt.str("m", "request method (GET, POST, HEAD)", &methodstring);

    char *postfile = NULL;
    opt.str("p", "path to the file with the POST body", &postfile);

    char **args = opt.parse(argc, argv);
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

	uint8_t *postdata = NULL;         /* *buffer containing data from postfile */
	size_t postlen = 0; /* length of data to be POSTed */

	int method = http.GET;
    if (methodstring) {
		method = http.method_from_string(methodstring);
		if (!method) {
            fprintf(stderr, "unknown request method: %s\n", methodstring);
            return 1;
		}
    }
    if (postfile) {
        if (method != http.POST) {
            fprintf(stderr, "postfile option works only with POST method\n");
            exit(1);
        }
        postdata = fs.readfile(postfile, &postlen);
        if (!postdata) {
            fprintf(stderr, "failed to read file %s: %s\n", postfile, strerror(errno));
            exit(1);
        }
    }
	const size_t MAX_CONCURRENCY = 20000;
    if (concurrency > MAX_CONCURRENCY) {
        fprintf(stderr, "concurrency is too high (%zu > %zu)\n", concurrency, MAX_CONCURRENCY);
        return 1;
    }
    if (concurrency > requests_to_do) {
        fprintf(stderr, "cannot use concurrency level greater than total number of requests\n");
        return 1;
    }

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

	//
    // Create the request.
	//
    http.request_t req = {};
    if (!http.init_request(&req, method, u.path)) {
		fprintf(stderr, "failed to init request: %s\n", http.errstr(req.err));
		return 1;
	}
    http.set_header(&req, "Host", u.hostname);
	http.set_header(&req, "User-Agent", "exab");
    if (method == http.POST) {
        char buf[10] = {0};
        sprintf(buf, "%zu", postlen);
        http.set_header(&req, "Content-Length", buf);
    }
    char buf[1000] = {0};
    if (!http.write_request(&req, buf, sizeof(buf))) {
        fatal("failed to write request");
    }
    io.push(&REQUEST, buf, strlen(buf));
    if (method == http.POST) {
        io.push(&REQUEST, (char *)postdata, postlen);
    }

	request_stats = calloc(requests_to_do, sizeof(result_t));
    context_t *connections = calloc(concurrency, sizeof(context_t));
    if (!connections || !request_stats) {
        panic("failed to allocate memory");
    }
	ioroutine.init();
	for (size_t i = 0; i < concurrency; i++) {
        connections[i].outbuf = io.newbuf();
        connections[i].inbuf = io.newbuf();
        ioroutine.spawn(routine, &connections[i]);
    }
    while (ioroutine.step()) {
        //
    }
    return 0;
}

int output_lines = 0;
void output(result_t *s) {
	if (!output_lines) {
		printf("#\tstatus\ttsending\ttreceiving\ttotaltime\tsent\treceived\n");
	}
	int64_t write = time.sub(s->time_finished_writing, s->time_started);
	int64_t read = time.sub(s->time_finished_reading, s->time_finished_writing);
	int64_t total = time.sub(s->time_finished_reading, s->time_started);
	printf("%d\t", output_lines++);
	if (s->error) {
		printf("error: %d (%s)\n", s->error, strerror(s->error));
		return;
	}
	printf("%d\t", s->status);
	printf("%f\t", (double) write / time.MS);
	printf("%f\t", (double) read / time.MS);
	printf("%f\t", (double) total / time.MS);
	printf("%zu\t", s->total_sent);
	printf("%zu\n", s->total_received);
}

enum {
	BEGIN,
    CONNECT,
    INIT_REQUEST,
    WRITE_REQUEST,
    READ_RESPONSE,
    READ_RESPONSE_BODY,
};

const char *DBG_TAG = "exab";

int routine(void *ctx, int line) {
    context_t *c = ctx;

    switch (line) {
		case BEGIN: {
			dbg.m(DBG_TAG, "getting next request: %zu", requests_done);
			c->stats = &request_stats[requests_done++];
			if (!c->connection) {
				return CONNECT;
			}
			return INIT_REQUEST;
		}
        /*
         * Connect to the remote host.
         */
        case CONNECT: {
            dbg.m(DBG_TAG, "connecting to %s", colonhost);
            c->connection = io.connect("tcp", colonhost);
            if (!c->connection) {
				c->stats->error = errno;
				output(c->stats);
				bad();
                return CONNECT;
            }
            dbg.m(DBG_TAG, "connected");
            return INIT_REQUEST;
        }
        case INIT_REQUEST: {
			dbg.m(DBG_TAG, "preparing the request (%zu)", io.bufsize(&REQUEST));
            io.resetbuf(c->inbuf);
			io.resetbuf(c->outbuf);
			io.push(c->outbuf, REQUEST.data, io.bufsize(&REQUEST));
            c->stats->time_started = time.now();
            return WRITE_REQUEST;
        }
        case WRITE_REQUEST: {
            size_t n = io.bufsize(c->outbuf);
            if (n == 0) {
                dbg.m(DBG_TAG, "nothing more to send, proceeding to receive");
                c->stats->time_finished_writing = time.now();
                return READ_RESPONSE;
            }
            if (!ioroutine.ioready(c->connection, io.WRITE)) {
                return WRITE_REQUEST;
            }
            dbg.m(DBG_TAG, "sending %zu bytes", n);
            if (!io.write(c->connection, c->outbuf)) {
				c->stats->error = errno;
                io.close(c->connection);
                c->stats->time_finished_writing = time.now();
				output(c->stats);
                return CONNECT;
            }
            c->stats->total_sent += n - io.bufsize(c->outbuf);
            return WRITE_REQUEST;
        }
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
				output(c->stats);
				bad();
                return CONNECT;
            }
            size_t n = io.bufsize(c->inbuf);
            dbg.m(DBG_TAG, "read %zu bytes", n - n0);
            c->stats->total_received += n - n0;
            if (n == 0) {
                io.close(c->connection);
                c->stats->time_finished_reading = time.now();
				output(c->stats);
                return CONNECT;
            }

            http.response_t r = {};
            if (!http.parse_response(c->inbuf->data, &r)) {
                // Assume not enough data was received.
                dbg.m(DBG_TAG, "waiting for more headers");
                return READ_RESPONSE;
            }
			c->stats->status = r.status;
            io.shift(c->inbuf, r.head_length);
            c->body_bytes_to_read = r.content_length;
            c->body_bytes_to_read -= io.bufsize(c->inbuf);
            io.shift(c->inbuf, io.bufsize(c->inbuf));
            return READ_RESPONSE_BODY;
        }
        case READ_RESPONSE_BODY: {
            return read_response_body(c);
        }
        default: {
            panic("unexpected line: %d", line);
        }
    }
    return -1;
}

int read_response_body(context_t *c) {
	if (c->body_bytes_to_read == 0) {
		dbg.m(DBG_TAG, "finished reading response, done=%zu, todo=%zu", requests_done, requests_to_do);
		c->stats->time_finished_reading = time.now();
		output(c->stats);
		if (requests_done >= requests_to_do) {
			dbg.m(DBG_TAG, "all done (todo=%zu)\n", requests_to_do);
			io.close(c->connection);
			return -1;
		}
		return BEGIN;
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

void fatal(char *s) {
    fprintf(stderr, "%s: %s (%d)\n", s, strerror(errno), errno);
    if (requests_done) {
        printf("Total of %zu requests completed\n" , requests_done);
    }
    exit(1);
}
