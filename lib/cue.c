/*
 * CUE format parser
 */
#import cli
#import parsebuf

/*
 * http://wiki.hydrogenaud.io/index.php?title=Cue_sheet
 */

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
#define MAXTRACKS 100

pub typedef {
	cuetrack_t tracks[MAXTRACKS];
	int ntracks;
} cue_t;

/*
 * Parsing context.
 * 'cmd' is the following CUE command (like "TRACK" or "TITLE")
 */
typedef {
	char cmd[40];
	parsebuf *buf;
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

/*
 * Parses given string, returns a 'cue_t' pointer.
 * Returns NULL on memory error.
 */
pub cue_t *cue_parse(const char *s)
{
	context_t c = {};

	c.buf = buf_new(s);
	cue_t *cue = calloc(1, sizeof(cue_t));
	if (!c.buf || !cue) {
		if (c.buf) buf_free(c.buf);
		return NULL;
	}

	/*
	 * Skip UTF-8 mark
	 */
	if((uint8_t) buf_peek(c.buf) == 0xEF) {
		buf_get(c.buf);
		if((uint8_t) buf_get(c.buf) != 0xBB
			|| (uint8_t) buf_get(c.buf) != 0xBF) {
			fatal("Unknown byte-order mark");
		}
	}

	/*
	 * Skip headers
	 */
	while(1) {
		read_command(&c);
		if(!is_in(c.cmd, head_props)) {
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
				fatal("Too many tracks (limit = %d)", MAXTRACKS);
			}
			read_track(&c, &cue->tracks[cue->ntracks]);
			cue->ntracks++;
		}
	}

	if(buf_more(c.buf)) {
		fatal("Unexpected command: %s", c.cmd);
	}

	buf_free(c.buf);
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

void read_track(context_t *c, cuetrack_t *track)
{
	skip_line(c->buf);

	char val[1000] = "";

	while(1) {
		read_command(c);
		if(!is_in(c->cmd, track_props)) {
			break;
		}

		int i = 0;
		while(buf_more(c->buf)) {
			int ch = buf_get(c->buf);
			val[i++] = ch;
			if(ch == '\n') break;
		}
		val[i] = '\0';

		if(strcmp(c->cmd, "TITLE") == 0) {
			/*
			 * Cut off double quotes
			 */
			if(val[0] != '"') {
				fatal("Double quotes expected in %s", val);
			}
			char *p = val + strlen(val);
			while(p > val) {
				if(*p == '"') {
					*p = '\0';
					break;
				}
				p--;
			}
			strcpy(track->title, val + 1);
			continue;
		}

		if(strcmp(c->cmd, "INDEX") == 0) {
			unsigned num = 0;
			unsigned min = 0;
			unsigned sec = 0;
			unsigned frames = 0;
			int n = sscanf(val, "%u %u:%u:%u", &num, &min, &sec, &frames);
			if(n != 4) {
				fatal("Couldn't parse index: %s", val);
			}
			if(num == 0) {
				continue;
			}
			if(num != 1) {
				fatal("Unexpected index number in %s", val);
			}
			const int us = 1000000;
			track->pos_usec = frames * us / 75 + sec * us + 60 * min * us;
			continue;
		}
	}
}

/*
 * Discards rest of the current line
 */
void skip_line(parsebuf *b)
{
	while(buf_more(b) && buf_peek(b) != '\n') {
		buf_get(b);
	}
	if(buf_peek(b) == '\n') {
		buf_get(b);
	}
}

/*
 * Reads the following CUE command into the 'cmd' field.
 */
void read_command(context_t *c)
{
	while(buf_peek(c->buf) == ' ' || buf_peek(c->buf) == '\t') {
		buf_get(c->buf);
	}
	int i = 0;
	while(isalpha(buf_peek(c->buf))) {
		c->cmd[i] = buf_get(c->buf);
		i++;
	}
	c->cmd[i] = '\0';
	while(isspace(buf_peek(c->buf))) {
		buf_get(c->buf);
	}
}

/*
 * Returns true if 'cmd' is in the NULL-terminated list
 * of strings 'list'.
 */
bool is_in(const char *cmd, const char **list)
{
	const char **p = list;
	while(*p) {
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
