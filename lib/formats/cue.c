/*
 * CUE format parser
 * http://wiki.hydrogenaud.io/index.php?title=Cue_sheet
 */
#import tokenizer
#import strings

/*
 * Track info: title and position in microseconds
 */
pub typedef {
	char title[300];
	uint64_t pos_usec;
} cuetrack_t;

/*
 * Sheet structure, the topmost element
 * with preallocated array of track info structures.
 */
const int MAXTRACKS = 100;

pub typedef {
	cuetrack_t tracks[100];
	int ntracks;
} cue_t;

/*
 * Parsing context.
 * 'cmd' is the following CUE command (like "TRACK" or "TITLE")
 */
typedef {
	char cmd[40];
	tokenizer.t *buf;
} context_t;

/*
 * Recognized header commands
 */
const char *head_props[] = {
	"REM",
	"PERFORMER",
	"DISCID",
	"TITLE",
	NULL
};

pub void cue_free(cue_t *c)
{
	free(c);
}

char __error[1000] = {};

/**
 * Returns a pointer to a static buffer with the given formatted message.
 * If the message is too long, it's truncated.
 */
char *makeerr(const char *fmt, ...) {
	va_list l = {0};
	va_start(l, fmt);
	vsnprintf(__error, sizeof(__error)-1, fmt, l);
	va_end(l);
	return __error;
}

/*
 * Parses the given string, returns a 'cue_t' pointer.
 * Returns NULL on error and sets `err` to point to an error message.
 */
pub cue_t *cue_parse(const char *s, char **err)
{
	context_t c = {};

	c.buf = tokenizer.from_str(s);
	if (!c.buf) {
		*err = strerror(errno);
		return NULL;
	}

	cue_t *cue = calloc(1, sizeof(cue_t));
	if (!cue) {
		tokenizer.free(c.buf);
		*err = strerror(errno);
		return NULL;
	}

	/*
	 * Skip the UTF-8 mark
	 */
	if((uint8_t) tokenizer.peek(c.buf) == 0xEF) {
		tokenizer.get(c.buf);
		if((uint8_t) tokenizer.get(c.buf) != 0xBB || (uint8_t) tokenizer.get(c.buf) != 0xBF) {
			free(cue);
			tokenizer.free(c.buf);
			*err = "Unknown byte-order mark";
			return NULL;
		}
	}

	// Skip headers
	while (true) {
		read_command(&c);
		if (!is_in(c.cmd, head_props)) {
			break;
		}
		skip_line(c.buf);
	}

	/*
	 * Read file->track trees
	 */
	while(strcmp(c.cmd, "FILE") == 0) {
		skip_line(c.buf);
		read_command(&c);
		while(strcmp(c.cmd, "TRACK") == 0) {
			if(cue->ntracks == MAXTRACKS) {
				free(cue);
				tokenizer.free(c.buf);
				*err = makeerr("too many tracks (limit = %d)", MAXTRACKS);
				return NULL;
			}
			if (!read_track(&c, &cue->tracks[cue->ntracks], err)) {
				free(cue);
				tokenizer.free(c.buf);
				return NULL;
			}
			cue->ntracks++;
		}
	}

	if(tokenizer.more(c.buf)) {
		free(cue);
		tokenizer.free(c.buf);
		*err = makeerr("unexpected command: '%s'", c.cmd);
		return NULL;
	}

	tokenizer.free(c.buf);
	return cue;
}

/*
 * Recognized title commands
 */
const char *track_props[] = {
	"TITLE",
	"PERFORMER",
	"INDEX",
	NULL
};

bool read_track(context_t *c, cuetrack_t *track, char **err)
{
	skip_line(c->buf);

	char val[500] = "";

	while (true) {
		read_command(c);
		if(!is_in(c->cmd, track_props)) {
			break;
		}

		int i = 0;
		while(tokenizer.more(c->buf)) {
			int ch = tokenizer.get(c->buf);
			val[i++] = ch;
			if(ch == '\n') break;
		}
		val[i] = '\0';

		if (strcmp(c->cmd, "TITLE") == 0) {
			// Trim double quotes.
			if (val[0] != '"') {
				*err = makeerr("double quotes expected in %s", val);
				return false;
			}
			strings.rtrim(val, "\"");
			strcpy(track->title, val + 1);
			continue;
		}

		if(strcmp(c->cmd, "INDEX") == 0) {
			size_t num = 0;
			size_t min = 0;
			size_t sec = 0;
			size_t frames = 0;
			int n = sscanf(val, "%lu %lu:%lu:%lu", &num, &min, &sec, &frames);
			if(n != 4) {
				*err = makeerr("couldn't parse index: %s", val);
				return false;
			}
			if(num == 0) {
				continue;
			}
			if(num != 1) {
				*err = makeerr("unexpected index number in %s", val);
				return false;
			}
			const int us = 1000000;
			track->pos_usec = frames * us / 75 + sec * us + 60 * min * us;
			continue;
		}
	}
	return true;
}

// Discards rest of the current line
void skip_line(tokenizer.t *b) {
	while (tokenizer.more(b) && tokenizer.peek(b) != '\n') {
		tokenizer.get(b);
	}
	if (tokenizer.peek(b) == '\n') {
		tokenizer.get(b);
	}
}

/*
 * Reads the following CUE command into the 'cmd' field.
 */
void read_command(context_t *c)
{
	while(tokenizer.peek(c->buf) == ' ' || tokenizer.peek(c->buf) == '\t') {
		tokenizer.get(c->buf);
	}
	int i = 0;
	while(isalpha(tokenizer.peek(c->buf))) {
		c->cmd[i] = tokenizer.get(c->buf);
		i++;
	}
	c->cmd[i] = '\0';
	while(isspace(tokenizer.peek(c->buf))) {
		tokenizer.get(c->buf);
	}
}

// Returns true if 'cmd' is in the NULL-terminated list
// of strings 'list'.
bool is_in(const char *cmd, const char **list) {
	const char **p = list;
	while (*p != NULL) {
		if(strcmp(*p, cmd) == 0) {
			return true;
		}
		p++;
	}
	return false;
}

pub cuetrack_t *cue_track(cue_t *c, int i)
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
