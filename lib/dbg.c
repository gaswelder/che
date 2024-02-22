#import os/misc

pub void m(const char *tag, *format, ...) {
	if (!dbg_enabled(tag)) return;
    printf("[%s] ", tag);
    va_list l = {0};
	va_start(l, format);
	vprintf(format, l);
	va_end(l);
    printf("\n");
}

bool dbg_enabled(const char *tag) {
	const char *val = misc.getenv("CHE_DEBUG");
	return val && strcmp(val, tag) == 0;
}
