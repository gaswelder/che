/*
 * CUE format parser
 * http://wiki.hydrogenaud.io/index.php?title=Cue_sheet
 */
#import tokenizer
#import time
#import error

#define MAXTRACKS 100

pub typedef {
	char title[300];
	time.duration_t pos;
} track_t;

pub typedef {
	track_t *tracks;
	int ntracks;
} cue_t;

typedef {
	int num;
	int min;
	int sec;
	int frames;
} index_t;

pub void cue_free(cue_t *c) {
	free(c->tracks);
	free(c);
}

// Parses cue sheet string s.
// Returns a cue_t instance.
pub cue_t *parse(const char *s, error.t *err) {
	cue_t *c = calloc!(1, sizeof(cue_t));
	c->tracks = calloc!(MAXTRACKS, sizeof(track_t));

	tokenizer.t *b = tokenizer.from_str(s);
	readcue(c, b, err);
	tokenizer.free(b);

	if (err->set) {
		cue_free(c);
		return NULL;
	}
	return c;
}

void readcue(cue_t *c, tokenizer.t *b, error.t *err) {
	track_t *t = NULL;
	// entry_t e = {};
	while (true) {
		if (!tokenizer.more(b)) {
			break;
		}

		tokenizer.hspaces(b);
		if (tokenizer.peek(b) == '\r') {
			tokenizer.get(b);
		}
		if (tokenizer.peek(b) == '\n') {
			tokenizer.get(b);
			continue;
		}

		// Read entry type.
		char type[20] = {};
		tokenizer.read_until(b, ' ', type, sizeof(type));
		tokenizer.hspaces(b);

		char content[1000] = {};
		switch str (type) {
			case "REM": {
				tokenizer.read_until(b, '\n', content, sizeof(content));
				if (tokenizer.peek(b) == '\n') tokenizer.get(b);
			}
			case "PERFORMER": {
				readtitle(b, content, sizeof(content));
				if (tokenizer.peek(b) == '\r') tokenizer.get(b);
				if (tokenizer.peek(b) == '\n') tokenizer.get(b);
			}
			case "TITLE": {
				readtitle(b, content, sizeof(content));
				if (tokenizer.peek(b) == '\r') tokenizer.get(b);
				if (tokenizer.peek(b) == '\n') tokenizer.get(b);
				// if t is null, this is the release title, ignore.
				// if t is not null, this is the track's title.
				if (t) {
					strcpy(t->title, content);
				}
			}
			case "TRACK": {
				tokenizer.read_until(b, '\n', content, sizeof(content));
				if (tokenizer.peek(b) == '\n') tokenizer.get(b);
				if (c->ntracks == MAXTRACKS) {
					error.set(err, "tracks limit reached (%d)", MAXTRACKS);
					return;
				}
				t = &c->tracks[c->ntracks++];
			}
			case "INDEX": {
				if (!t) {
					error.set(err, "unexpected index entry");
					return;
				}
				index_t index = {};
				readindex(b, &index, err);

				// Only index "01" is the actual track position.
				// "00" is "pregap", "02" and higher are markers within the track.
				if (index.num == 1) {
					t->pos = index_pos(&index);
				}
				if (tokenizer.peek(b) == '\n') tokenizer.get(b);
			}
			case "FILE": {
				// readtitle(b, content, sizeof(content));
				// tokenizer.read_until(b, '\n', e.data.file.kind, sizeof(e.data.file.kind));
				tokenizer.read_until(b, '\n', content, sizeof(content));
				if (tokenizer.peek(b) == '\r') tokenizer.get(b);
				if (tokenizer.peek(b) == '\n') tokenizer.get(b);
			}
			default: {
				panic("unknown entry type: '%s'", type);
			}
		}
	}
}

// Reads a title into buf.
void readtitle(tokenizer.t *b, char *buf, size_t n) {
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

// Reads an index string:
// 01 01:12:00
void readindex(tokenizer.t *b, index_t *r, error.t *err) {
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
		error.set(err, "couldn't parse index: %s", val);
		return;
	}
	if (r->num == 0) {
		return;
	}
}

time.duration_t index_pos(index_t *r) {
	int sec = (r->frames / 75) + (r->sec) + (60 * r->min);
	time.duration_t p = {};
	time.dur_set(&p, sec, time.SECONDS);
	return p;
}

// Returns the absolute position of track t as microseconds.
pub int64_t pos_us(track_t *t) {
	return time.dur_us(&t->pos);
}
