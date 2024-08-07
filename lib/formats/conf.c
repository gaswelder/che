// Reads key-value config files.
// A line may contain up to one pair in form "key = value".
// Everything after # is a comment.

pub typedef {
	int line;
	FILE *f;
    char key[100];
    char val[200];
} reader_t;

int peek(FILE *f) {
    char c = getc(f);
    if (c != EOF) ungetc(c, f);
    return c;
}

/*
 * Allocates a new conf reader for the given file stream.
 */
pub reader_t *new(FILE *f) {
    reader_t *p = calloc(1, sizeof(reader_t));
    if (!p) return NULL;
    p->f = f;
    p->line = 1;
    return p;
}

pub void free(reader_t *p) {
    OS.free(p);
}

/*
 * Reads in the next key-value pair.
 */
pub bool next(reader_t *p) {
	char line[256] = {};
	while (!feof (p->f)) {
		next_line(p, line, sizeof(line));
		if (line[0] == '\0') {
			continue;
		}
		// process line
		char *base = line;
		char *ptr = base;
		while (*ptr != '=' && *ptr != ' ' && *ptr != 0) ptr++;
		if (*ptr == 0) {
			fprintf (stderr, "config parse error at line %d\n", p->line);
			exit (1);
		}
		*ptr = 0;

		/* Now find value */
		ptr++;
		while (*ptr == '=' || *ptr == ' ') ptr++;
		if (*ptr == 0) {
			fprintf (stderr, "config parse error at line %d\n", p->line);
			exit (1);
		}

		strcpy(p->key, base);
		strcpy(p->val, ptr);
		return 1;
	}
	return 0;
}

void next_line(reader_t *p, char *buf, size_t bufsize) {
	char *ptr = buf;
	bool comment = false;
	size_t pos = 0;

    // Skip spaces.
    while (isspace(peek(p->f))) getc(p->f);

	while (!feof (p->f)) {
		char c = getc (p->f);
		if (c == EOF) {
			*ptr = 0;
			return;
		}
		if (c == '#') {
			comment = true;
			continue;
		}
		if (c == 10) {
			*ptr = 0;
			p->line++;
			return;
		}
		if (!comment) {
			pos++;
			if (pos >= bufsize) {
				panic("buf too small");
			}
			*ptr++ = c;
		}
	}
	*ptr = 0;
	return;
}
