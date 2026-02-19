/*
 * CUE format parser
 * http://wiki.hydrogenaud.io/index.php?title=Cue_sheet
 */
#import tokenizer
#import time

#define MAXTRACKS 100

pub typedef {
	bool set;
	char msg[100];
} err_t;

void seterr(err_t *err, const char *fmt, ...) {
	va_list args = {};
	va_start(args, fmt);
	vsnprintf(err->msg, sizeof(err->msg), fmt, args);
	va_end(args);
	err->set = true;
}

pub typedef {
	char title[300];
	time.duration_t pos;
} track_t;

pub typedef {
	track_t tracks[100];
	int ntracks;
} cue_t;

pub void cue_free(cue_t *c) {
	free(c);
}

// Parses cue sheet string s.
// Returns a cue_t instance.
pub cue_t *parse(const char *s, err_t *err) {
	cue_t *c = calloc!(1, sizeof(cue_t));
	tokenizer.t *b = tokenizer.from_str(s);
	readbuf(c, b, err);
	tokenizer.free(b);
	if (err->set) {
		OS.free(c);
		return NULL;
	}
	return c;
}

void readbuf(cue_t *c, tokenizer.t *b, err_t *err) {
	if (!skip_utf_bom(b)) {
		seterr(err, "Unknown byte-order mark");
		return;
	}
	while (tokenizer.skip_literal(b, "REM ")) {
		line(b);
	}
	if (tokenizer.skip_literal(b, "PERFORMER ")) {
		line(b);
	}
	if (tokenizer.skip_literal(b, "TITLE ")) {
		line(b);
	}

	// FILE "..." WAVE
	readfile(b, err);
	if (err->set) {
		return;
	}

	while (tokenizer.more(b)) {
		if (c->ntracks == MAXTRACKS) {
			panic("too many tracks (limit = %d)", MAXTRACKS);
		}
		track_t *t = &c->tracks[c->ntracks++];
		readtrack(t, b, err);
		if (err->set) return;
	}
}

// FILE "..." WAVE
void readfile(tokenizer.t *b, err_t *err) {
	char buf[500] = {};
	if (!tok(b, "FILE", err)) return;
	title(b, buf, sizeof(buf));
	if (!tokenizer.skip_literal(b, " MP3") && !tokenizer.skip_literal(b, " WAVE")) {
		seterr(err, "expected MP3 or WAVE at the file entry");
		return;
	}
	line(b);
}

void readtrack(track_t *t, tokenizer.t *b, err_t *err) {
	char buf[500] = {};

	// TRACK 01 AUDIO
	if (!tok(b, "TRACK", err)) return;
	num(b);
	if (!tok(b, "AUDIO", err)) return;
	line(b);

	// TITLE "..."
	if (!tok(b, "TITLE", err)) return;
	title(b, t->title, sizeof(t->title));
	line(b);

	// PERFORMER "..."
	if (!tok(b, "PERFORMER", err)) return;
	title(b, buf, sizeof(buf));
	line(b);

	// INDEX 01 01:12:00
	if (!tok(b, "INDEX", err)) return;
	index_t r = {};
	index(b, &r);
	t->pos = index_pos(&r);
}

// Skips a specific token or panics and returns true.
// If the token doesn't follow, sets the error and returns false.
bool tok(tokenizer.t *b, const char *s, err_t *err) {
	tokenizer.hspaces(b);
	if (!tokenizer.skip_literal(b, s)) {
		char buf[10] = {};
		tokenizer.tail(b, buf, 9);
		seterr(err, "expected %s, got %s...", s, buf);
		return false;
	}
	tokenizer.hspaces(b);
	return true;
}

// Reads a number.
int num(tokenizer.t *b) {
	int n = 0;
	bool ok = false;
	while (tokenizer.more(b) && isdigit(tokenizer.peek(b))) {
		n *= 10;
		n += tokenizer.get(b) - (int)'0';
		ok = true;
	}
	if (!ok) {
		panic("expected a number");
	}
	return n;
}

// Reads a title into buf.
void title(tokenizer.t *b, char *buf, size_t n) {
	if (tokenizer.get(b) != '"') {
		panic("double quotes expected");
	}
	if (!tokenizer.read_until(b, '"', buf, n)) {
		panic("failed to read title");
	}
	if (tokenizer.get(b) != '"') {
		panic("double quotes expected");
	}
}

// Reads from b until the next line starts.
void line(tokenizer.t *b) {
	while (tokenizer.more(b) && tokenizer.peek(b) != '\n') {
		tokenizer.get(b);
	}
	if (tokenizer.peek(b) == '\n') {
		tokenizer.get(b);
	}
}

typedef {
	int num;
	int min;
	int sec;
	int frames;
} index_t;

void index(tokenizer.t *b, index_t *r) {
	// 01 01:12:00

	char val[300] = {};
	int i = 0;
	while (tokenizer.more(b)) {
		int ch = tokenizer.get(b);
		val[i++] = ch;
		if(ch == '\n') break;
	}
	val[i] = '\0';

	int n = sscanf(val, "%d %d:%d:%d", &r->num, &r->min, &r->sec, &r->frames);
	if (n != 4) {
		panic("couldn't parse index: %s", val);
	}
	if (r->num == 0) {
		return;
	}
	if (r->num != 1) {
		panic("unexpected index number: %s", val);
	}
}

time.duration_t index_pos(index_t *r) {
	int sec = (r->frames / 75) + (r->sec ) + (60 * r->min);
	time.duration_t p = {};
	time.dur_set(&p, sec, time.SECONDS);
	return p;
}

// Skips the sequence EF BB BF, if it follows.
// Returns true on success or when there is no mark.
// Returns false on an unexpected EF.. sequence.
bool skip_utf_bom(tokenizer.t *b) {
	if ((uint8_t) tokenizer.peek(b) != 0xEF) {
		return true;
	}
	tokenizer.get(b);
	return (
		(uint8_t) tokenizer.get(b) == 0xBB
		&& (uint8_t) tokenizer.get(b) == 0xBF
	);
}

pub track_t *cue_track(cue_t *c, int i)
{
	if(i < 0 || i >= c->ntracks) {
		return NULL;
	}
	return &c->tracks[i];
}

pub int cue_ntracks(cue_t *c)
{
	return c->ntracks;
}

// Returns the absolute position of track t as microseconds.
pub int64_t pos_us(track_t *t) {
	return time.dur_us(&t->pos);
}
