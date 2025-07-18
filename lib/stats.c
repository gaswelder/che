#import rnd

pub typedef {
    double *values;
    size_t len;
    size_t cap;

    // Cache
    double *sorted_values;
    double min;
    double max;
} series_t;

pub series_t *newseries() {
    return calloc!(1, sizeof(series_t));
}

pub void freeseries(series_t *s) {
    if (s->sorted_values) {
        free(s->sorted_values);
    }
    free(s->values);
    free(s);
}

pub void add(series_t *s, double val) {
    if (s->len == s->cap) {
        if (!grow(s)) {
            panic("failed to allocate memory");
        }
    }

    // Update the cache.
    if (s->sorted_values) {
        free(s->sorted_values);
        s->sorted_values = NULL;
    }
    if (s->len == 0 || val < s->min) {
        s->min = val;
    }
    if (s->len == 0 || val > s->max) {
        s->max = val;
    }

    // Insert the value.
    s->values[s->len++] = val;
}

/*
 * Returns the number of values in the series.
 */
pub size_t count(series_t *s) {
	return s->len;
}

pub double min(series_t *s) {
    return s->min;
}
pub double max(series_t *s) {
    return s->max;
}

/*
 * Returns sum of the values in the series.
 */
pub double sum(series_t *s) {
	double r = 0;
	for (size_t i = 0; i < s->len; i++) {
        r += s->values[i];
    }
	return r;
}

/*
 * Returns arithmetic average of the values in the series.
 */
pub double avg(series_t *s) {
    return sum(s) / s->len;
}

pub double sd(series_t *s) {
    double m = avg(s);
    double varsum = 0;
    for (size_t i = 0; i < s->len; i++) {
        double dev = m - s->values[i];
        varsum += dev * dev;
    }
    return sqrt(varsum / (s->len-1));
}

pub double percentile(series_t *s, int p) {
    if (p < 0 || p > 100) {
        return 0;
    }
    if (!s->sorted_values) {
        double *tmp = calloc!(s->len, sizeof(double));
        for (size_t i = 0; i < s->len; i++) {
            tmp[i] = s->values[i];
        }
        qsort(tmp, s->len, sizeof(double), doublecmp);
        s->sorted_values = tmp;
    }
    double val = s->sorted_values[(int) (s->len * p / 100)];
    return val;
}

pub double median(series_t *s) {
    return percentile(s, 50);
}

int doublecmp(const void *x, *y) {
    const double *a = x;
    const double *b = y;
    if (*a > *b) return 1;
    if (*a < *b) return -1;
    return 0;
}

bool grow(series_t *a) {
	size_t newcap = a->cap * 2;
	if (!newcap) {
        newcap = 16;
    }
	void *new = realloc(a->values, sizeof(double) * newcap);
	if (!new) {
        return false;
    }
	a->values = new;
	a->cap = newcap;
	return true;
}

// Normalizes the list xs of length n so that the sum of all elements is 1.
pub void normalize(double *xs, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        sum += xs[i];
    }
    for (size_t i = 0; i < n; i++) {
        xs[i] /= sum;
    }
}

// Samples the list of observation frequencies and returns an index from that list.
// For example, for a list [100, 1254, 3],
// it will return index 1 with probability 1254/(1254+100+3).
pub int sample_from_distribution(size_t *freqs, size_t n) {
	size_t sum = 0;
	for (size_t i = 0; i < n; i++) {
		sum += freqs[i];
	}
    size_t x = (size_t) rnd.intn(sum);
	int r = -1;
    // Here we calculate cumulative probabilities implicitly using
    // just one sum.
    size_t cdf = 0;
    for (size_t i = 0; i < n; i++) {
        cdf += freqs[i];
        if (cdf > x) {
            r = (int) i;
            break;
        }
    }
    if (r < 0) panic("oopsie, couldn't choose");
    return r;
}
