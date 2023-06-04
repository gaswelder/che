int magic(char *fmt, ...) {
    va_list l = {};
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
}
