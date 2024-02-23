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
	if (!parsed) {
		parse();
		parsed = true;
	}
	for (size_t i = 0; i < n; i++) {
		if (strcmp(list[i], tag) == 0) {
			return true;
		}
	}
	return false;
}

bool parsed = false;
const char *list[10] = {};
size_t n = 0;

void parse() {
	const char *val = misc.getenv("CHE_DEBUG");
	if (!val) return;

	char *copy = calloc(strlen(val)+1, 1);
	strcpy(copy, val);
	char *p = copy;
	list[n++] = p;
	while (*p) {
		if (*p == ',') {
			*p = '\0';
			if (n == 9) break;
			list[n++] = p+1;
		}
		p++;
	}
}
