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

	printf("intr %7.2f ± %.2f\n", stats.avg(s), stats.sd(s));
	printf(
		"n\t%zu\n"
		"sum\t%f\n"
		"min\t%f\n"
		"avg\t%f\n"
		"max\t%f\n",
		stats.count(s),
		stats.sum(s),
		stats.min(s),
		stats.avg(s),
		stats.max(s)
	);

	int percs[] = {50, 66, 75, 80, 90, 95, 98, 99, 100};
	for (size_t i = 0; i < nelem(percs); i++) {
		int p = percs[i];
		if (p == 100) {
			printf("p100\t%.1f\n", stats.max(s));
		} else {
			printf("p%d\t%.1f\n", p, stats.percentile(s, p));
		}
	}
	return 0;
}
