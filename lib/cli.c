
/*
 * Print a formatted message to stderr and exit with non-zero status.
 */
pub void fatal(const char *fmt, ...)
{
	va_list l = {0};
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	va_end(l);
	fprintf(stderr, "\r\n");
	exit(1);
}

/*
 * Print a formatter message to stderr.
 */
pub void err(const char *fmt, ...)
{
	va_list l = {0};
	va_start(l, fmt);
	vfprintf(stderr, fmt, l);
	fprintf(stderr, "\r\n");
	va_end(l);
}
