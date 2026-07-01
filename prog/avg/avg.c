#import opt
#import time

int main(int argc, char *argv[]) {
	size_t wsize = 5;
	opt.summary("Applies sliding window average to the input values.");
	opt.nargs(0, "");
	opt.size("w", "window size", &wsize);
	opt.parse(argc, argv);

	line_t *window = calloc!(wsize, sizeof(line_t));
	char line[4096];

	FILE *f = stdin;

	// Fill the window. We'll discard the first values.
	for (size_t i = 0; i < wsize; i++) {
		if (!fgets(line, 4096, f)) {
			return 0;
		}
		window[i] = parse_line(line);
	}
	emit(window, wsize);


	while (fgets(line, 4096, f)) {
		// Shift the window.
		for (size_t i = 0; i < wsize-1; i++) {
			window[i] = window[i+1];
		}
		window[wsize-1] = parse_line(line);
		emit(window, wsize);
	}
	return 0;
}

void emit(line_t *window, int wsize) {
	double sum = 0;
	for (int i = 0; i < wsize; i++) {
		sum += (double) window[i].bps;
	}
	char buf[100];
	time.fmt_iso_iso(window[wsize/2].ts, buf, 100);
	printf("%s\t%.1f\n", buf, sum/wsize);
}

typedef {
	time.iso_t ts;
	int bps;
} line_t;

line_t parse_line(char *line) {
	// Split into two columns.
	char *ts = line;
	char *p = line;
	while (*p != '\0' && *p != '\t') {
		p++;
	}
	*p = '\0';
	p++;

	// Parse as (time, int)
	line_t r = {};
	r.ts = time.parse_iso_(ts);
	sscanf(p, "%d", &r.bps);
	return r;
}
