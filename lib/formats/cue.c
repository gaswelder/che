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
	readcue(c, b, err);
	tokenizer.free(b);
	if (err->set) {
		OS.free(c);
		return NULL;
	}
	return c;
}

void readcue(cue_t *c, tokenizer.t *b, err_t *err) {
	track_t *t = NULL;
	entry_t e = {};
	while (readentry(b, &e)) {
		switch str (e.type) {
			case "REM": {} // ignore
			case "PERFORMER": {} // ignore
			case "FILE": {} // ignore
			case "TITLE": {
				// if t is null, this is the release title, ignore.
				// if t is not null, this is the track's title.
				if (t) {
					strcpy(t->title, e.data.title);
				}
			}
			case "TRACK": {
				if (c->ntracks == MAXTRACKS) {
					seterr(err, "tracks limit reached (%d)", MAXTRACKS);
					return;
				}
				t = &c->tracks[c->ntracks++];
			}
			case "INDEX": {
				if (!t) {
					seterr(err, "unexpected index entry");
					return;
				}
				t->pos = index_pos(&e.data.index);
			}
			default: {
				panic("unknown entry type: %s", e.type);
			}
		}
	}
}

typedef {
	int n;
	char kind[20];
} trackentry_t;

typedef {
	char path[100];
	char kind[20];
} file_t;

typedef {
	int num;
	int min;
	int sec;
	int frames;
} index_t;

typedef {
	char type[100];
	union {
		char rem[1000];
		char title[100];
		char performer[100];
		file_t file;
		trackentry_t track;
		index_t index;
	} data;
} entry_t;


bool readentry(tokenizer.t *b, entry_t *e) {
	if (!tokenizer.more(b)) {
		return false;
	}
	tokenizer.hspaces(b);
	tokenizer.read_until(b, ' ', e->type, sizeof(e->type));
	tokenizer.hspaces(b);

	switch str (e->type) {
		case "REM": {
			tokenizer.read_until(b, '\n', e->data.rem, sizeof(e->data.rem));
		}
		case "PERFORMER": {
			title(b, e->data.performer, sizeof(e->data.performer));
		}
		case "TITLE": {
			title(b, e->data.title, sizeof(e->data.title));
		}
		case "TRACK": {
			e->data.track.n = num(b);
			tokenizer.hspaces(b);
			tokenizer.read_until(b, '\n', e->data.track.kind, sizeof(e->data.track.kind));
		}
		case "INDEX": {
			index(b, &e->data.index);
		}
		case "FILE": {
			title(b, e->data.file.path, sizeof(e->data.file.path));
			tokenizer.hspaces(b);
			tokenizer.read_until(b, '\n', e->data.file.kind, sizeof(e->data.file.kind));
		}
		default: {
			panic("unknown entry type: %s", e->type);
		}
	}
	if (tokenizer.peek(b) == '\r') {
		tokenizer.get(b);
	}
	if (tokenizer.peek(b) == '\n') {
		tokenizer.get(b);
	}
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
