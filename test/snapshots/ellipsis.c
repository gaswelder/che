int magic(char *fmt, ...) {
    va_list l = {0};
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
}
