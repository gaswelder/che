#import os/self

bool env_parsed = false;
const char *list[10] = {};
size_t n = 0;


const char *default_filter = NULL;

// Sets the default debug filter.
// Might be convenient during development to not have to set the env var.
// The env var will still take precedence if set.
pub void set(const char *s) {
	default_filter = s;
}

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
	const char *val = self.getenv("CHE_DEBUG");
	if (!val) val = self.getenv("DEBUG");
	if (!val) val = default_filter;
	if (!val) return;

	char *copy = calloc(strlen(val)+1, 1);
	strcpy(copy, val);
	char *p = copy;
	list[n++] = p;
	while (*p != '\0') {
		if (*p == ',') {
			*p = '\0';
			if (n == 9) break;
			list[n++] = p+1;
		}
		p++;
	}
}

// Debug utility to print bytes.
pub void print_bytes(uint8_t *data, size_t n) {
    printf("\t --------- %zu bytes ---------------------------\n", n);
    int w = 0;
	for (size_t i = 0; i < n; i++) {
        if (w == 0) printf("\t ");
        if (w >= 60) {
            printf("\n\t ");
            w = 0;
        }
		char c = data[i];
		if (isprint(c)) {
            w++;
			printf("%c", c);
		} else if (c == '\r') {
			printf("\\r");
		} else if (c == '\n') {
			printf("\\n");
		} else {
            w += 2;
			printf(" %x ", (uint8_t)(int)c);
		}
	}
	printf("\n\t --------- end data ----------------------------\n");
}
