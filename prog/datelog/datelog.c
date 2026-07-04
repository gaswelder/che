#import opt
#import time

const size_t MAXPATH = 512;

int main(int argc, char *argv[]) {
	bool output = false;
	opt.nargs(0, "");
	opt.flag("o", "output lines to stdout", &output);
	opt.parse(argc, argv);

	dlog_t *log = dlog_init();
	if (!log) {
		return 1;
	}

	char buf[BUFSIZ] = {};
	while (fgets(buf, sizeof(buf), stdin)) {
		if (output) fprintf(stdout, "%s", buf);
		int err = dlog_put(log, buf);
		if (err) {
			fprintf(stderr, "Failed to write the log. err=%d, os error=%s.\n", err, strerror(errno));
			dlog_free(log);
			return 1;
		}
	}
	dlog_free(log);
	return 0;
}

typedef {
	char *current_path;
	char *new_path;
	FILE *fp;
} dlog_t;

enum {
	E_FOPEN = 1, // "Could not open file '%s' for writing\n"
	E_FPUTS = 2, //
}

dlog_t *dlog_init() {
	dlog_t *log = calloc!(1, sizeof(dlog_t));
	log->current_path = calloc!(1, MAXPATH+1);
	log->new_path = calloc!(1, MAXPATH+1);
	return log;
}

void dlog_free(dlog_t *log) {
	if (log->fp) fclose(log->fp);
	free(log->current_path);
	free(log->new_path);
	free(log);
}

int dlog_put(dlog_t *log, const char *line) {
	time.iso_t now = time.now();
	sprintf(log->new_path, "%d-%02d.log", now.Y, now.M);

	// If the file name hasn't changed, return the current file.
	if (!log->fp || strcmp(log->new_path, log->current_path)) {
		if (log->fp) {
			fclose(log->fp);
		}
		strcpy(log->current_path, log->new_path);
		log->fp = fopen(log->current_path, "ab+");
		if (!log->fp) {
			return E_FOPEN;
		}
	}
	if (fputs(line, log->fp) == EOF) {
		return E_FPUTS;
	}
	return 0;
}
