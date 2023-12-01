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
	opt.opt_summary("reads numeric values from stdin and prints an ASCII histogram");
	opt.opt_size("n", "number of bins", &nbins);
	opt.opt_size("w", "max line width", &maxline);
	opt.opt_bool("h", "show help", &help);
	opt.opt_parse(argc, argv);

	if (help) {
		opt.opt_usage();
		return 1;
	}

	if (nbins == 0) {
		fprintf(stderr, "the number of bins must be > 0\n");
		return 1;
	}

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

	bin_t *bins = calloc(nbins, sizeof(bin_t));
	if (!bins) {
		panic("failed to get memory");
	}

	double binwidth = (stats.max(s) - stats.min(s)) / nbins;
	double x = stats.min(s);
	for (size_t i = 0; i < nbins; i++) {
		bins[i].a = x;
		x += binwidth;
		bins[i].b = x;
	}

	for (size_t i = 0; i < s->len; i++) {
		double x = s->values[i];

		// This weird search is because the max value doesn't
		// always "match" the last bin because of the floating
		// point behavior.
		bin_t *b = NULL;
		for (size_t pos = 0; pos < nbins; pos++) {
			b = &bins[pos];
			if (x >= b->a && x <= b->b) {
				break;
			}
		}
		b->count++;
	}

	printbins(bins, nbins, maxline);

	return 0;
}

void printbins(bin_t *bins, size_t nbins, size_t maxline) {
	size_t maxcount = 0;
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		if (b->count > maxcount) {
			maxcount = b->count;
		}
	}
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		printf("%f--%f\t", b->a, b->b);
		size_t len = 0;
		if (maxcount > 0 && b->count > 0) {
			len = b->count * maxline / maxcount;
		}
		line(len);
		if (b->count > 0) {
			printf(" %zu\n", b->count);
		} else {
			printf("\n");
		}
	}
}

void line(size_t len) {
	for (size_t i = 0; i < len; i++) {
		putchar('=');
	}
}
