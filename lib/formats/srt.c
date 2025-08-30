#import strings
#import time

pub typedef {
	char linebuf[4096];
	bool line_loaded;
} reader_t;

pub typedef {
	int index;
	char range[40];
	char text[1000];
	time.duration_t begin, end;
} block_t;

pub bool block(reader_t *r, block_t *b) {
	if (feof(stdin)) {
		return false;
	}
	if (!r->line_loaded) {
		loadline(r);
		r->line_loaded = true;
	}

	b->index = parse_num(r->linebuf);
	loadline(r);

	strcpy(b->range, r->linebuf);
	loadline(r);

	b->text[0] = '\0';
	while (true) {
		if (parse_num(r->linebuf)) break;
		if (strlen(b->text) + strlen(r->linebuf) >= sizeof(b->text)) {
			panic("text buffer is too small");
		}
		strcat(b->text, r->linebuf);
		if (!loadline(r)) break;
	}
	strings.trim(b->range);
	strings.trim(b->text);


	//
	// Parse the range into two time positions
	//
	int h1;
	int m1;
	int s1;
	int ms1;
	int h2;
	int m2;
	int s2;
	int ms2;
	// 00:01:32,320 --> 00:01:33,160
	sscanf(b->range, "%d:%d:%d,%d --> %d:%d:%d,%d", &h1, &m1, &s1, &ms1, &h2, &m2, &s2, &ms2);
	b->begin = mkpos(h1, m1, s1, ms1);
	b->end = mkpos(h2, m2, s2, ms2);
	return true;
}

time.duration_t mkpos(int h, m, s, ms) {
	int val = h;
	val *= 60; // m
	val += m;
	val *= 60; // s
	val += s;
	val *= 1000; // ms
	val += ms;
	return time.newdur(val, time.MS);
}

bool loadline(reader_t *r) {
	char *p = r->linebuf;
	while (true) {
		int c = fgetc(stdin);
		if (c == EOF) break;
		if (c == '\r') continue;
		*p++ = c;
		if (c == '\n') break;
	}
	if (p == (char *) r->linebuf) {
		return false;
	}
	*p++ = '\0';
	return true;
}

int parse_num(char *s) {
	int n = 0;
	char *p = s;
	while (isdigit(*p)) {
		n *= 10;
		n += strings.num_from_ascii(*p);
		p++;
	}
	while (isspace(*p)) p++;
	if (*p != '\0') {
		return 0;
	}
	return n;
}
