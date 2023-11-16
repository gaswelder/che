#import stats

int main() {
	stats.series_t *s = stats.newseries();

	double val = 0;
	while (true) {
		int r = scanf("%lf\n", &val);
		if (r == EOF) {
			break;
		}
		if (r != 1) {
			fprintf(stderr, "failed to parse a number\n");
			return 1;
		}
		stats.add(s, val);
	}

	printf(
		"%f ± %f\n"
		"n = %zu\n"
		"min = %f\n"
		"max = %f\n"
		"sum = %f\n"
		"avg = %f\n"
		"p50 = %f\n"
		"p95 = %f\n"
		"p99 = %f\n",
		stats.avg(s), stats.sd(s),
		stats.count(s),
		stats.min(s),
		stats.max(s),
		stats.sum(s),
		stats.avg(s),
		stats.percentile(s, 50),
		stats.percentile(s, 95),
		stats.percentile(s, 99)
	);
	return 0;
}
