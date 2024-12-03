#import strings

pub typedef {
	char line[4096];
	FILE *f;
	bool init;
	char *text;
	size_t textcap;
} reader_t;

pub reader_t *reader(FILE *f) {
	reader_t *r = calloc(1, sizeof(reader_t));
	if (!r) panic("calloc failed");
	r->textcap = 100;
	r->text = calloc(100, 1);
	if (!r->text) panic("calloc failed");
	r->f = f;
	return r;
}

pub void freereader(reader_t *r) {
	free(r->text);
	free(r);
}

bool nextline(reader_t *r) {
	if (!fgets(r->line, sizeof(r->line), r->f)) {
		return false;
	}
	strings.rtrim(r->line, "\r\n");
	return true;
}

// Reads the next subtitle.
pub bool read(reader_t *r) {
	if (!r->init) {
		r->init = true;
		if (!init(r)) return false;
	}

	if (!nextline(r) || !cue(r->line)) return false;

	// text
	if (r->text) r->text[0] = '\0';
	while (true) {
		if (!nextline(r)) return false;
		if (r->line[0] == '\0') break;
		if (strlen(r->text) + strlen(r->line) + 2 > r->textcap) {
			r->textcap *= 2;
			r->text = realloc(r->text, r->textcap);
			if (!r->text) panic("realloc failed");
		}
		if (r->text[0]) strcat(r->text, "\n");
		printnotags(r->text + strlen(r->text), r->line);
	}
	return true;
}

void printnotags(char *dest, const char *s) {
	const char *p = s;
	bool tag = false;
	while (*p) {
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

bool init(reader_t *r) {
	if (!fgets(r->line, sizeof(r->line), r->f)) {
		return false;
	}
	strings.trim(r->line);
	if (strcmp(r->line, "WEBVTT")) {
		return false;
	}
	while (true) {
		if (!fgets(r->line, sizeof(r->line), r->f)) {
			return false;
		}
		strings.rtrim(r->line, "\r\n");
		if (r->line[0] == '\0') break;
		if (!header(r->line)) {
			// printf("failed to read a header: '%s'\n", r->line);
			return false;
		}
	}
	return true;
}

// Kind: captions
bool header(char *line) {
	char *delim = strstr(line, ": ");
	if (!delim) return false;
	*delim = '\0';
	// printf("header: %s = %s\n", line, delim + 2);
	return true;
}

// 00:00:00.120 --> 00:00:02.270 align:start position:0%
bool cue(char *line) {
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
	// printf("timestamp: %s\n", ts);
	return p;
}
