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

// Reads a subtitle block from r into b.
pub bool readblock(reader_t *r, block_t *b) {
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
	// 00:01:32,320 --> 00:01:33,160
	char sbegin[20] = {};
	char send[20] = {};
	sscanf(b->range, "%s --> %s", sbegin, send);	
	b->begin = parsepos(sbegin);
	b->end = parsepos(send);
	return true;
}

// Parses a timestamp in srt format: 00:01:33,160.
pub time.duration_t parsepos(const char *buf) {
	int h;
	int m;
	int s;
	int ms;
	if (sscanf(buf, "%d:%d:%d,%d", &h, &m, &s, &ms) != 4) {
		panic("failed to parse timestamp");
	}

	int val = h;
	val *= 60; // m
	val += m;
	val *= 60; // s
	val += s;
	val *= 1000; // ms
	val += ms;
	return time.newdur(val, time.MS);
}

pub void printblock(block_t *b) {
	char t1[20] = {};
	char t2[20] = {};
	time.dur_fmt(&b->begin, t1, 20, "hh:mm:ss,ms");
	time.dur_fmt(&b->end, t2, 20, "hh:mm:ss,ms");
	printf("%d\n", b->index);
	printf("%s --> %s\n", t1, t2);
	printf("%s\n", b->text);
	printf("\n");
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
