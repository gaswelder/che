int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: stats <stats-list>\n"
		"	<stats-list> if a sequence of characers that specify which stats to calculate:"
		"	a - average\n"
		"	s - sum\n"
		"	m - min\n"
		"	x - max\n"
		"	n - count\n");
		return 1;
	}

	size_t num = 0;
	double avg = 0;
	double sum = 0;
	double min = 0;
	bool min_init = false;
	double max = 0;
	bool max_init = false;

	bool avg_needed = false;
	bool sum_needed = false;
	bool min_needed = false;
	bool max_needed = false;

	for (char *p = argv[1]; *p; p++) {
		switch (*p) {
			case 'a': { avg_needed = true; }
			case 's': { sum_needed = true; }
			case 'm': { min_needed = true; }
			case 'x': { max_needed = true; }
			case 'n': {}
			default: {
				fprintf(stderr, "unknown stats character: %c\n", *p);
				return 1;
			}
		}
	}

	while (true) {
		double val = 0;
		int r = scanf("%lf\n", &val);
		if (r == EOF) {
			break;
		}
		if (r != 1) {
			fprintf(stderr, "failed to parse the number\n");
			return 1;
		}

		num++;
		if (sum_needed) sum += val;
		if (avg_needed) avg = avg * (num-1)/num + val/num;
		if (min_needed) {
			if (min_init == false) {
				min = val;
				min_init = true;
			} else if (val < min) {
				min = val;
			}
		}
		if (max_needed) {
			if (max_init == false) {
				max = val;
				max_init = true;
			} else if (val > max) {
				max = val;
			}
		}
	}

	for (char *p = argv[1]; *p; p++) {
		if (p > argv[1]) {
			printf("\t");
		}
		switch (*p) {
			case 's': { printf("%f", sum); }
			case 'a': { printf("%f", avg); }
			case 'n': { printf("%lu", num); }
			case 'm': { printf("%f", min); }
			case 'x': { printf("%f", max); }
			default: { panic("!"); }
		}
	}
	printf("\n");
	return 0;
}
