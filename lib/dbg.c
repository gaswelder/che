#import os/misc

bool env_parsed = false;
const char *list[10] = {};
size_t n = 0;

/**
 * Makes a debug printf under the given tag.
 * If the tag is not given in the DEBUG env var list, ignores the message.
 */
pub void m(const char *tag, *format, ...) {
	if (!dbg_enabled(tag)) return;
	size_t n = strlen(tag) + 3;
	printf("[%s] ", tag);
	if (n < 15) {
		size_t r = 15 - n;
		for (size_t i = 0; i < r; i++) putchar(' ');
	}
    va_list l = {0};
	va_start(l, format);
	vprintf(format, l);
	va_end(l);
    printf("\n");
}

bool dbg_enabled(const char *tag) {
	if (!env_parsed) {
		parse_env();
		env_parsed = true;
	}
	for (size_t i = 0; i < n; i++) {
		if (strcmp(list[i], tag) == 0) return true;
		if (strcmp(list[i], "_") == 0) return true;
	}
	return false;
}

void parse_env() {
	const char *val = misc.getenv("CHE_DEBUG");
	if (!val) val = misc.getenv("DEBUG");
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
