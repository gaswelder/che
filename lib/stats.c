pub typedef {
    double *values;
    size_t len;
    size_t cap;

    double min;
    double max;
} series_t;

pub series_t *newseries() {
    return calloc(1, sizeof(series_t));
}

pub void add(series_t *s, double val) {
    if (s->len == s->cap) {
        if (!grow(s)) {
            panic("failed to allocate memory");
        }
    }
    if (s->len == 0 || val < s->min) {
        s->min = val;
    }
    if (s->len == 0 || val > s->max) {
        s->max = val;
    }
    s->values[s->len++] = val;
}

pub double min(series_t *s) {
    return s->min;
}
pub double max(series_t *s) {
    return s->max;
}
pub double mean(series_t *s) {
    double sum = 0;
    for (size_t i = 0; i < s->len; i++) {
        sum += s->values[i];
    }
    return sum / s->len;
}
pub double sd(series_t *s) {
    double m = mean(s);
    double varsum = 0;
    for (size_t i = 0; i < s->len; i++) {
        double dev = m - s->values[i];
        varsum += dev * dev;
    }
    return sqrt(varsum / (s->len-1));
}
pub double median(series_t *s) {
    double *tmp = calloc(s->len, sizeof(double));
    for (size_t i = 0; i < s->len; i++) {
        tmp[i] = s->values[i];
    }
    qsort(tmp, s->len, sizeof(double), doublecmp);
    double m = (tmp[s->len / 2] + tmp[s->len/2 + 1]) / 2;
    free(tmp);
    return m;
}

int doublecmp(const void *x, *y) {
    const double *a = x;
    const double *b = y;
    if (*a > *b) return 1;
    if (*a < *b) return -1;
    return 0;
}

pub void freeseries(series_t *s) {
    free(s->values);
    free(s);
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
