#import dbg
#import http
#import opt
#import os/ioloop
#import reader
#import strings
#import time
#import url
#import writer

const char *DBG_TAG = "exab";

typedef {
    time.t time_started;
    time.t time_finished_writing;
    time.t time_finished_reading;
	int status;
	size_t total_received;
	size_t total_sent;
	int error;

	char response[1000];
	size_t responselen;
} request_t;

request_t *requests = NULL;
size_t requests_done = 0;
size_t requests_to_do = 1;

char *addr = NULL;
uint8_t REQUEST[1000] = {};
size_t REQUESTLEN = 0;


int main(int argc, char *argv[]) {
	size_t concurrency = 1;

	opt.nargs(1, "<url>");
    opt.opt_summary("makes a series of HTTP requests to <url> and prints statistics");
    opt.size("n", "number of requests to perform", &requests_to_do);
    opt.size("c", "number of requests running concurrently", &concurrency);

    char **args = opt.parse(argc, argv);
	char *urlstr = *args;
	init_request(urlstr);
	requests = calloc(requests_to_do, sizeof(request_t));
	if (!requests) panic("calloc failed");
	for (size_t i = 0; i < concurrency; i++) {
		next();
    }
	while (ioloop.process()) {
		//
	}
    return 0;
}

void init_request(const char *urlstr) {
	url.t *u = url.parse(urlstr);
	if (!u) {
        fprintf(stderr, "failed to parse URL: %s\n", urlstr);
        exit(1);
    }
    if (!strcmp(u->schema, "https")) {
        fprintf(stderr, "no https support\n");
        exit(1);
    }
    if (u->port[0] == '\0') {
        strcpy(u->port, "80");
    }
    addr = strings.newstr("%s:%s", u->hostname, u->port);

	http.request_t req = {};
    if (!http.init_request(&req, http.GET, u->path)) {
		fprintf(stderr, "failed to init request: %s\n", http.errstr(req.err));
		exit(1);
	}
    http.set_header(&req, "Host", u->hostname);
	http.set_header(&req, "User-Agent", "exab");

	writer.t *w = writer.static_buffer(REQUEST, sizeof(REQUEST));
    if (!http.write_request(w, &req)) {
        panic("failed to write the request");
    }
	REQUESTLEN = w->nwritten;
	writer.free(w);
}

void next() {
	if (requests_done >= requests_to_do) {
		dbg.m(DBG_TAG, "no more requests");
		return;
	}
	dbg.m(DBG_TAG, "getting next request: %zu", requests_done);
	request_t *req = &requests[requests_done];
	requests_done++;
	ioloop.connect(addr, handler, req);
}

void handler(void *ctx, int event, void *data) {
	switch (event) {
		case ioloop.CONNECTED: {
			request_t *req = data;
			ioloop.set_stash(ctx, data);
            req->time_started = time.now();
			if (!ioloop.write(ctx, (char *)REQUEST, REQUESTLEN)) {
				panic("write failed");
			}
			req->total_sent = REQUESTLEN;
		}
		case ioloop.DATA_IN: {
			request_t *req = ioloop.get_stash(ctx);

			// Append the data to the buffer.
			ioloop.buff_t *buf = data;
			for (size_t i = 0; i < buf->len; i++) {
				if (req->responselen == 1000) {
					panic("response buffer too small");
				}
				req->response[req->responselen++] = buf->data[i];
			}

			// Try to parse the response.
			http.response_t r = {};
			reader.t *re = reader.string(req->response);
            if (!http.parse_response(re, &r)) {
                // Assume not enough data was received.
                dbg.m(DBG_TAG, "waiting for more data");
				reader.free(re);
				return;
            }
			reader.free(re);
			req->status = r.status;
			req->total_received = req->responselen;
			req->time_finished_reading = time.now();
			dbg.m(DBG_TAG, "finished reading response, done=%zu, todo=%zu", requests_done, requests_to_do);
			done(req);
		}
		case ioloop.WRITE_FINISHED: {
			request_t *req = ioloop.get_stash(ctx);
			req->time_finished_writing = time.now();
		}
		case ioloop.EXIT: {
			next();
		}
		default: {
			panic("got event %d", event);
		}
	}
}

int output_lines = 0;
void done(request_t *s) {
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
