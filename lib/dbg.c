#import os/self
#import writer
#import reader

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

	char *copy = calloc!(strlen(val)+1, 1);
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
pub void print_bytes(const uint8_t *data, size_t n) {
    printf("--------- %zu bytes ---------------------------\n", n);
	printf("0\t");
	for (size_t i = 0; i < n; i++) {
		if (i > 0 && i % 20 == 0) {
			printf("\n%zu\t", i);
		}
		printf(" %02x", data[i]);
		// if (isprint((char) data[i])) {
		// 	printf(" %c", data[i]);
		// }
	}
	printf("\n--------- end data ----------------------------\n");
}


//
// echo writer
//

typedef {
	writer.t *out;
} dbgwriter_t;

int dbgwrite(void *ctx, const uint8_t *data, size_t n) {
	dbgwriter_t *w = ctx;
	printf("## writing bytes:");
	for (size_t i = 0; i < n; i++) {
		printf(" %u", data[i]);
	}
	printf("\n");
	return writer.write(w->out, data, n);
}

pub writer.t *newwriter(writer.t *out) {
	dbgwriter_t *w = calloc!(1, sizeof(dbgwriter_t));
	w->out = out;
	return writer.new(w, dbgwrite, OS.free);
}

//
// echo reader
//

typedef {
	reader.t *in;
} dbgreader_t;

pub reader.t *newdbgreader(reader.t *in) {
	dbgreader_t *w = calloc!(1, sizeof(dbgreader_t));
	w->in = in;
	return reader.new(w, dbgread, OS.free);
}

int dbgread(void *ctx, uint8_t *data, size_t n) {
	dbgreader_t *w = ctx;
	int r = reader.read(w->in, data, n);
	if (r < 0) {
		printf("## read failed\n");
	} else {
		printf("## read bytes:");
		for (int i = 0; i < r; i++) {
			printf(" %u", data[i]);
		}
		printf("\n");
	}
	return r;
}
