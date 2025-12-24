#import stats
#import opt

int main(int argc, char *argv[]) {
	size_t nbins = 10;
	size_t maxline = 30;
	bool raw = false;
	opt.nargs(0, "");
	opt.summary("Reads numeric values from stdin and prints their histogram.");
	opt.size("n", "number of bins (excluding two padding bins); if zero, will be set to sqrt(n_values)", &nbins);
	opt.size("w", "max line width", &maxline);
	opt.flag("r", "print raw numbers, no fancy ASCII art", &raw);
	opt.parse(argc, argv);
	if (nbins != 0 && nbins < 2) {
		fprintf(stderr, "The number of bins must be greater than two\n");
		return 1;
	}
	stats.series_t *s = collect();

	bin_t *bins = group_bins(s, nbins);
	if (raw) {
		printbins(bins, nbins);
	} else {
		printbins_ascii(bins, nbins, maxline);
	}
	return 0;
}

// Reads input numbers, returns them as series.
stats.series_t *collect() {
	stats.series_t *s = stats.newseries();
	double val = 0;
	while (true) {
		int r = scanf("%lf\n", &val);
		if (r == EOF) break;
		if (r != 1) {
			fprintf(stderr, "failed to parse a number\n");
			exit(1);
		}
		stats.add(s, val);
	}
	return s;
}

typedef {
	double minval;
	size_t count;
} bin_t;

typedef {
	double min, step;
} range_t;

range_t find_range(double _min, _max, size_t nbins) {
	double scale = pow(10, floor(log10(fabs(_min))));
	if (scale == 0) {
		scale = 0.000001;
	}
	double amin = 0;
	double amax = 0;
	while (true) {
		amin = floor(_min / scale) * scale;
		amax = amin + nbins * scale;
		if (amax >= _max) {
			break;
		}
		scale *= 10;
	}
	while (amin + nbins * (scale/2) >= _max) {
		scale /= 2;
	}
	range_t r = { .min = amin, .step = scale };
	return r;
}

bin_t *group_bins(stats.series_t *s, size_t nbins) {
	range_t r = find_range(stats.min(s), stats.max(s), nbins);

	bin_t *bins = calloc!(nbins, sizeof(bin_t));
	for (size_t i = 0; i < nbins; i++) {
		bins[i].minval = r.min + i * r.step;
	}

	for (size_t i = 0; i < s->len; i++) {
		double x = s->values[i];
		bin_t *b = findbin(bins, nbins, x);
		if (!b) {
			panic("failed to find bin for %g", x);
		}
		b->count++;
	}
	return bins;
}

bin_t *findbin(bin_t *bins, size_t nbins, double val) {
	bin_t *b = NULL;
	for (size_t i = 0; i < nbins; i++) {
		bin_t *next = &bins[i];
		if (val >= next->minval) {
			b = next;
		} else {
			break;
		}
	}
	return b;
}

void printbins(bin_t *bins, size_t nbins) {
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		printf("%g\t%zu\n", b->minval, b->count);
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
	label_t *labels = calloc!(nbins, sizeof(label_t));
	size_t maxlabel = 0;
	for (size_t i = 0; i < nbins; i++) {
		bin_t *b = &bins[i];
		label_t *l = &labels[i];
		size_t n = snprintf(l->str, sizeof(l->str), "%g", b->minval);
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
	printf("value (>=)");
	repeat("-", maxlabel + maxline - strlen("value (>=)") + 1);
	printf(" count\n");

	double scale = 30 / (double) maxcount;
	if (scale > 1) {
		scale = 1;
	}

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
		len = (size_t) ((double)b->count * scale);
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
