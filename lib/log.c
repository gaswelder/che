#import time

pub bool logmsg(const char *fmt, ...) {
	time.iso_t t = time.now();
	time.iso_tolocal(&t);
	fprintf(stderr, "%d-%02d-%02d %02d:%02d:%02d.%03d ", t.Y, t.M, t.D, t.h, t.m, t.s, t.ms);

	va_list args = {};
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fputc('\n', stderr);
	return true;
}
