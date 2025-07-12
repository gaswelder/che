/*
 * CUE format parser
 * http://wiki.hydrogenaud.io/index.php?title=Cue_sheet
 */
#import tokenizer
#import time

#define MAXTRACKS 100

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

// Parses cue sheet in the string s.
// Returns a cue_t instance.
// The caller must check cur->error for errors.
pub cue_t *cue_parse(const char *s) {
	cue_t *c = calloc!(1, sizeof(cue_t));
	tokenizer.t *b = tokenizer.from_str(s);

	if (!skip_utf_bom(b)) {
		panic("Unknown byte-order mark");
	}

	char buf[500] = {};

	tok(b, "FILE");
	title(b, buf, sizeof(buf));
	tok(b, "MP3");
	line(b);

	while (tokenizer.more(b)) {
		if (c->ntracks == MAXTRACKS) {
			panic("too many tracks (limit = %d)", MAXTRACKS);
		}

		track_t *t = &c->tracks[c->ntracks++];

		// TRACK 01 AUDIO
		tok(b, "TRACK");
		num(b);
		tok(b, "AUDIO");
		line(b);

		// TITLE "..."
		tok(b, "TITLE");
		title(b, t->title, sizeof(t->title));
		line(b);

		// PERFORMER "..."
		tok(b, "PERFORMER");
		title(b, buf, sizeof(buf));
		line(b);

		// INDEX 01 01:12:00
		tok(b, "INDEX");
		index_t r = {};
		index(b, &r);
		t->pos = index_pos(&r);
	}

	tokenizer.free(b);
	return c;
}

// Skips a specific token or panics.
void tok(tokenizer.t *b, const char *s) {
	tokenizer.hspaces(b);
	if (!tokenizer.skip_literal(b, s)) {
		panic("expected %s", s);
	}
	tokenizer.hspaces(b);
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
pub int pos_us(track_t *t) {
	return time.dur_us(&t->pos);
}
