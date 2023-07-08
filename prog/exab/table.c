
pub void add(const char *title, const char *fmt, ...) {
    printf("%-30s", title);
    va_list l = {0};
	va_start(l, fmt);
	vprintf(fmt, l);
	va_end(l);
    printf("\n");
}

pub void split() {
    printf("\n");
}