#import stats
#import opt

typedef {
	double a, b;
	size_t count;
} bin_t;

int main(int argc, char *argv[]) {
	size_t nbins = 10;
	size_t maxline = 30;
	bool help = false;
	bool raw = false;
	opt.opt_summary("reads numeric values from stdin and prints their histogram");
	opt.size("n", "number of bins (excluding two padding bins)", &nbins);
	opt.size("w", "max line width", &maxline);
	opt.flag("r", "print raw numbers, no fancy ASCII art", &raw);
	opt.flag("h", "show help", &help);
	opt.opt_parse(argc, argv);

	if (help) return opt.usage();

	if (nbins < 2) {
		fprintf(stderr, "The number of bins must be greater than two\n");
		return 1;
	}

	//
	// Collect the values.
	//
	stats.series_t *s = stats.newseries();
	double val = 0;
	while (true) {
		int r = scanf("%lf\n", &val);
		if (r == EOF) break;
		if (r != 1) {
			fprintf(stderr, "failed to parse a number\n");
			return 1;
		}
		stats.add(s, val);
	}

	//
	// Group into bins.
	//
	bin_t *bins = calloc(nbins+2, sizeof(bin_t));
	if (!bins) {
		panic("failed to get memory");
	}
	double _min = stats.min(s);
	double _max = stats.max(s);
	double step = (_max - _min) / (nbins-1);

	// 1 bin for >= MIN-STEP
	bins[0].a = _min - step;
	bins[0].b = _min;

	// n bins for >=MIN, >=MIN+STEP, ...
	for (size_t i = 0; i < nbins; i++) {
		bins[i+1].a = _min + i*step;
		bins[i+1].b = _min + (i+1)*step;
	}

	// 1 bin for >= MAX+STEP
	bins[nbins+1].a = _max + step;
	bins[nbins+1].b = _max + step + step;

	for (size_t i = 0; i < s->len; i++) {
		double x = s->values[i];

		// This weird search is because the max value doesn't
		// always "match" the last bin because of the floating
		// point behavior.
		bin_t *b = NULL;
		for (size_t pos = 0; pos < nbins+2; pos++) {
			b = &bins[pos];
			if (x >= b->a && x < b->b) {
				break;
			}
		}
		b->count++;
	}

	if (raw) {
		printbins(bins, nbins+2);
	} else {
		printbins_ascii(bins, nbins+2, maxline);
	}

	return 0;
}

void printbins(bin_t *bins, size_t nbins) {
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		printf("%f\t%zu\n", (b->a + b->b)/2, b->count);
	}
}

typedef {
	char str[100];
} label_t;

void printbins_ascii(bin_t *bins, size_t nbins, size_t maxline) {
	//
	// Print the labels to a buffer in advance so that we know
	// label sizes.
	//
	label_t *labels = calloc(nbins, sizeof(label_t));
	size_t maxlabel = 0;
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		label_t *l = &labels[i];
		size_t n = snprintf(l->str, sizeof(l->str), "%f", b->a);
		if (n > maxlabel) maxlabel = n;
	}

	//
	// Find out the max count across bins.
	//
	size_t maxcount = 0;
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		if (b->count > maxcount) {
			maxcount = b->count;
		}
	}

	//
	// Header.
	//
	printf("value ");
	repeat("-", maxlabel + maxline - strlen("value") + 1);
	printf(" count\n");

	//
	// Output the bin lines.
	//
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		//
		// label
		//
		repeat(" ", maxlabel - strlen(labels[i].str));
		printf("%s", labels[i].str);
		printf(" |");

		//
		// bar
		//
		size_t len = 0;
		if (maxcount > 0 && b->count > 0) {
			len = b->count * maxline / maxcount;
		}
		repeat("█", len);
		repeat(" ", maxline-len);

		//
		// value
		//
		printf(" %zu\n", b->count);
	}


	free(labels);
}

void repeat(const char *s, size_t n) {
	for (size_t i = 0; i < n; i++) {
		printf("%s", s);
	}
}
