#import dbg
#import opt
#import os/net
#import os/threads
#import protocols/http
#import reader
#import strings
#import time
#import url
#import writer

const char *DBG_TAG = "exab";

char *addr = NULL;
uint8_t REQUEST[1000] = {};
size_t REQUESTLEN = 0;

threads.mtx_t *stdout_lock = NULL;

typedef {
	size_t id;
	threads.pipe_t *pipe;
} worker_arg_t;

int main(int argc, char *argv[]) {
	stdout_lock = threads.mtx_new();
	size_t concurrency = 1;
	size_t requests_to_do = 1;
	opt.nargs(1, "<url>");
	opt.summary("makes a series of HTTP requests to <url> and prints statistics");
	opt.size("n", "number of requests to perform", &requests_to_do);
	opt.size("c", "number of requests running concurrently", &concurrency);
	char **args = opt.parse(argc, argv);
	char *urlstr = *args;

	//
	// Parse and check the URL.
	//
	url.t *u = url.parse(urlstr);
	if (!u) {
		fprintf(stderr, "failed to parse URL: %s\n", urlstr);
		return 1;
	}
	if (!strcmp(u->schema, "https")) {
		fprintf(stderr, "no https support\n");
		return 1;
	}
	if (u->port[0] == '\0') {
		strcpy(u->port, "80");
	}
	addr = strings.newstr("%s:%s", u->hostname, u->port);

	//
	// Create and format the HTTP request as a bytes buffer.
	//
	http.request_t req = {};
	if (!http.init_request(&req, http.GET, u->path)) {
		fprintf(stderr, "failed to init request: %s\n", http.errstr(req.err));
		return 1;
	}
	http.set_header(&req, "Host", u->hostname);
	http.set_header(&req, "User-Agent", "exab");
	writer.t *w = writer.static_buffer(REQUEST, sizeof(REQUEST));
	if (!http.write_request(w, &req)) {
		panic("failed to write the request");
	}
	REQUESTLEN = w->nwritten;
	writer.free(w);

	// Adjust the workers number if there is not enough work for all.
	if (concurrency > requests_to_do) {
		concurrency = requests_to_do;
	}

	threads.pipe_t *pipe = threads.newpipe();
	threads.thr_t **tt = calloc!(concurrency, sizeof(threads.thr_t *));
	worker_arg_t *aa = calloc!(concurrency, sizeof(worker_arg_t));

	print_result_header();
	for (size_t i = 0; i < concurrency; i++) {
		aa[i].id = i;
		aa[i].pipe = pipe;
		tt[i] = threads.start(&worker, &aa[i]);
	}

	for (size_t i = 0; i < requests_to_do; i++) {
		// Sending NULL because there is nothing to send, we only
		// need to nudge a worker to make another request.
		threads.pwrite(pipe, NULL);
	}
	threads.pclose(pipe);

	for (size_t i = 0; i < concurrency; i++) {
		threads.wait(tt[i], NULL);
	}
	free(aa);
	free(tt);
	threads.freepipe(pipe);
	return 0;
}

void *worker(void *arg) {
	dbg.m(DBG_TAG, "thread started");
	worker_arg_t *a = arg;
	threads.pipe_t *pipe = a->pipe;

	char resbytes[4096] = {};

	result_t _res = {};
	result_t *res = &_res;
	res->worker_id = a->id;

	size_t i = 0;
	while (true) {
		if (!threads.pread(pipe, NULL)) {
			break;
		}
		res->number = i;
		res->time_started = time.now();
		net.net_t *conn = net.connect("tcp", addr);
		if (!conn) {
			panic("failed to connect to %s: %s", addr, strerror(errno));
		}
		res->time_connected = time.now();
		if ((size_t) net.write(conn, (char *)REQUEST, REQUESTLEN) != REQUESTLEN) {
			panic("failed to write request: %s", strerror(errno));
		}
		res->total_sent = REQUESTLEN;
		res->time_finished_writing = time.now();
		int read = net.read(conn, resbytes, sizeof(resbytes));
		if (read < 0) {
			panic("read failed");
		}
		if ((size_t) read == sizeof(resbytes)) {
			panic("read buffer too small");
		}
		resbytes[read] = '\0';
		res->time_finished_reading = time.now();
		res->total_received = read;
		net.close(conn);
		http.response_t r = {};
		reader.t *re = reader.string(resbytes);
		if (!http.parse_response(re, &r)) {
			panic("failed to parse http response");
		}
		reader.free(re);
		res->status = r.status;
		print_result(res);

		i++;
	}
	return NULL;
}

typedef {
	size_t worker_id;
	size_t number;
	time.t time_started;
	time.t time_connected;
	time.t time_finished_writing;
	time.t time_finished_reading;
	int status;
	size_t total_received;
	size_t total_sent;
} result_t;

void print_result_header() {
	printf("#\tstatus\ttconnecting\ttsending\ttreceiving\ttotaltime\tsent\treceived\n");
}

void print_result(result_t *res) {
	threads.lock(stdout_lock);
	int64_t conn = time.sub(res->time_connected, res->time_started);
	int64_t write = time.sub(res->time_finished_writing, res->time_started);
	int64_t read = time.sub(res->time_finished_reading, res->time_finished_writing);
	int64_t total = time.sub(res->time_finished_reading, res->time_started);
	printf("%zu/%zu\t", res->worker_id, res->number);
	printf("%d\t", res->status);
	printf("%f\t", (double) conn / time.MS);
	printf("%f\t", (double) write / time.MS);
	printf("%f\t", (double) read / time.MS);
	printf("%f\t", (double) total / time.MS);
	printf("%zu\t", res->total_sent);
	printf("%zu\n", res->total_received);
	threads.unlock(stdout_lock);
}