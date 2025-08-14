pub typedef {
	size_t ncols;
	size_t pos;
	char **values;
	size_t cap;
} table_t;

pub table_t *new(size_t ncols) {
	table_t *t = calloc!(1, sizeof(table_t));
	t->ncols = ncols;
	t->cap = 100;
	t->values = calloc!(ncols * 100, sizeof(char *));
	return t;
}

pub void pushval(table_t *t, char *format, ...) {
	if (t->pos == t->cap) {
		t->cap += 100;
		char **n = realloc(t->values, t->cap * sizeof(table_t));
		if (!n) panic("realloc failed");
		t->values = n;
	}
	va_list args = {};
	va_start(args, format);
	int len = vsnprintf(NULL, 0, format, args);
	va_end(args);
	if (len < 0) {
		panic("failed to format the string");
	}
	char *s = calloc!(len + 1, 1);
	va_start(args, format);
	vsnprintf(s, len + 1, format, args);
	va_end(args);

	t->values[t->pos++] = s;
}

pub void print(table_t *t) {
	size_t *widths = calloc!(t->ncols, sizeof(size_t));

	for (size_t i = 0; i < t->pos; i++) {
		size_t col = i % t->ncols;
		size_t len = strlen(t->values[i]);
		if (len > widths[col]) {
			widths[col] = len + 2;
		}
	}

	for (size_t i = 0; i < t->pos; i++) {
		size_t col = i % t->ncols;
		printwidth(t->values[i], widths[col]);
		if (col == t->ncols-1) {
			putchar('\n');
		}
	}

	OS.free(widths);
}

pub void free(table_t *t) {
	for (size_t i = 0; i < t->pos; i++) {
		OS.free(t->values[i]);
	}
	OS.free(t->values);
	OS.free(t);
}

void printwidth(char *s, size_t w) {
	size_t n = 0;
	while (*s != '\0') {
		putchar(*s);
		s++;
		n++;
		if (n == w) break;
	}
	while (n < w) {
		putchar(' ');
		n++;
	}
}
