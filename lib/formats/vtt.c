#import strings

pub typedef {
	char line[4096];
	FILE *f;
	char timerange[100];
	char *text;
	size_t textcap;
	char *error;
} reader_t;

// Creates a VTT reader for file f.
pub reader_t *reader(FILE *f) {
	reader_t *r = calloc!(1, sizeof(reader_t));
	r->textcap = 100;
	r->text = calloc!(100, 1);
	r->f = f;
	if (!init(r)) {
		seterror(r, "failed to initialize");
	}
	return r;
}

// Frees the reader.
pub void freereader(reader_t *r) {
	if (r->error) {
		free(r->error);
	}
	free(r->text);
	free(r);
}

// Returns the error encountered by the reader.
pub char *err(reader_t *r) {
	return r->error;
}

bool init(reader_t *r) {
	// WEBVTT
	if (!loadline(r)) return false;
	if (strcmp(r->line, "WEBVTT") != 0) {
		seterror(r, "expected WEBVTT at line 1");
		return false;
	}

	// A sequence of "k: v" headers.
	while (true) {
		if (!loadline(r)) return false;

		// Stop at an empty line.
		if (r->line[0] == '\0') break;

		char *delim = strstr(r->line, ": ");
		if (!delim) {
			seterror(r, "failed to parse header: %s", r->line);
			return false;
		}
		*delim = '\0';
	}
	return true;
}

// Reads the next subtitle.
pub bool read(reader_t *r) {
	// Skip empty lines
	while (true) {
		if (!loadline(r)) return false;
		if (r->line[0] != '\0') break;
	}

	// 00:09:53.000 --> 00:09:55.910 align:start position:0%
	if (!cue(r->line, r->timerange)) {
		seterror(r, "failed to parse cue line: %s", r->line);
		return false;
	}

	// Text lines
	if (r->text) r->text[0] = '\0';
	while (true) {
		if (!loadline(r)) return false;

		// Stop at an empty line.
		if (r->line[0] == '\0') break;

		if (strlen(r->text) + strlen(r->line) + 2 > r->textcap) {
			r->textcap *= 2;
			r->text = realloc(r->text, r->textcap);
			if (!r->text) panic("realloc failed");
		}
		if (r->text[0]) strcat(r->text, "\n");
		printnotags(r->text + strlen(r->text), r->line);
	}
	strings.trim(r->text);
	return true;
}

bool loadline(reader_t *r) {
	if (!fgets(r->line, sizeof(r->line), r->f)) {
		return false;
	}
	strings.rtrim(r->line, "\r\n");
	// printf("line: [%s]\n", r->line);
	return true;
}

void printnotags(char *dest, const char *s) {
	const char *p = s;
	bool tag = false;
	while (*p != '\0') {
		if (tag) {
			if (*p == '>') {
				tag = false;
			}
		} else {
			if (*p == '<') {
				tag = true;
			} else {
				*dest++ = *p;
			}
		}
		p++;
	}
	*dest = '\0';
}

void seterror(reader_t *r, char *msg, ...) {
	if (r->error) {
		return;
	}
	char buf[1000] = {};
	va_list args = {};
	va_start(args, msg);
	vsnprintf(buf, sizeof(buf)-1, msg, args);
	va_end(args);
	r->error = strings.newstr("%s", buf);
}

// 00:00:00.120 --> 00:00:02.270 align:start position:0%
bool cue(char *line, char *out) {
	char *p = line;
	p = timestamp(p);
	if (!p) return false;
	if (*p++ != ' ') return false;
	if (strstr(p, "--> ") != p) return false;
	p += 4;
	p = timestamp(p);
	if (!p) return false;
	if (*p == ' ') {
		p++;
		// printf("style: %s\n", p);
	}
	size_t n = p - line;
	for (size_t i = 0; i < n; i++) {
		out[i] = line[i];
	}
	out[n] = '\0';
	return true;
}


// 00:00:00.120
char *timestamp(char *s) {
	char *p = s;
	char ts[20] = {};
	char *r = ts;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (*p != ':') return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (*p != ':') return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (*p != '.') return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	if (!isdigit(*p)) return NULL; *r++ = *p++;
	return p;
}
