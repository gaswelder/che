#import stats

int main() {
	double vals[10] = {};

	// Find out the number of columns.
	size_t nseries = readvals(vals);
	if (nseries == 0) {
		panic("no values on the first line");
	}

	// Initialize the series from the already read values.
	stats.series_t *series[10] = {};
	for (size_t i = 0; i < nseries; i++) {
		series[i] = stats.newseries();
		stats.add(series[i], vals[i]);
	}

	while (true) {
		size_t n = readvals(vals);
		if (n == 0 && feof(stdin)) {
			break;
		}
		if (n != nseries) {
			panic("expected %zu values, got %zu", nseries, n);
		}
		for (size_t i = 0; i < n; i++) {
			stats.add(series[i], vals[i]);
		}
	}

	print_results(series, nseries);
	return 0;
}

size_t readvals(double *vals) {
	char line[4096] = {};
	if (!fgets(line, sizeof(line), stdin)) {
		return 0;
	}
	char *p = line;
	char *q = NULL;
	size_t n = 0;
	while (true) {
		double x = strtod(p, &q);
		if (q == p) {
			break;
		}
		vals[n++] = x;
		p = q;
	}
	return n;
}

void print_results(stats.series_t *series[], size_t nseries) {
	printf("avg");
	for (size_t i = 0; i < nseries; i++) {
		printf("\t%7.2f ± %.2f", stats.avg(series[i]), stats.sd(series[i]));
	}
	putchar('\n');
	printf("min");
	for (size_t i = 0; i < nseries; i++) {
		printf("\t%.2f", stats.min(series[i]));
	}
	putchar('\n');
	printf("max");
	for (size_t i = 0; i < nseries; i++) {
		printf("\t%.2f", stats.max(series[i]));
	}
	putchar('\n');

	printf("N");
	for (size_t i = 0; i < nseries; i++) {
		printf("\t%zu", stats.count(series[i]));
	}
	putchar('\n');

	printf("sum");
	for (size_t i = 0; i < nseries; i++) {
		printf("\t%f", stats.sum(series[i]));
	}
	putchar('\n');

	int percentiles[] = {5, 25, 33, 50, 66, 75, 95, 99};
	for (size_t i = 0; i < nelem(percentiles); i++) {
		int p = percentiles[i];
		printf("0.%02d", p);
		for (size_t j = 0; j < nseries; j++) {
			printf("\t%.1f", stats.percentile(series[j], p));
		}
		putchar('\n');
	}
}
