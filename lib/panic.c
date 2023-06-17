pub void panic(const char *fmt, ...) {
	fprintf(stderr, "*** panic ***\n");
	va_list l = {0};
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
	fprintf(stderr, "\r\n");
	exit(1);
}
