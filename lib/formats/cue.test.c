#import cue.c
#import time
#import test
#import error

const char *s = "REM GENRE \"Avant-garde Metal\"
REM DATE 2006
REM DISCID A911310C
REM COMMENT \"ExactAudioCopy v0.95b4\"
PERFORMER \"Celtic Frost\"
TITLE \"Monotheist (Century Media, USA, 8282-2, Limited edition)\"
FILE \"A Mind Confused - A Mind Confused (1995).mp3\" MP3
TRACK 01 AUDIO
  TITLE \"Eternal Sleep\"
  PERFORMER \"A Mind Confused\"
  INDEX 01 00:00:00
  INDEX 02 00:02:00
TRACK 02 AUDIO
  TITLE \"Dreams of an Erotic Salvation\"
  PERFORMER \"A Mind Confused\"
  INDEX 00 04:23:00
  INDEX 01 04:27:00
TRACK 03 AUDIO
  TITLE \"A Mind Confused\"
  PERFORMER \"A Mind Confused\"
  INDEX 01 08:57:00

\r

";

int main() {
	error.t err = {};
    cue.cue_t *c = cue.parse(s, &err);
	if (err.set) {
		panic("failed to parse: %s", err.msg);
	}
	if (!c) panic("null return");

    test.truth("ntracks", c->ntracks == 3);

    char buf[10] = {};
    char line[400] = {};
    
    int i = 0;
    cue.track_t *t = NULL;

    t = &c->tracks[i++];
    time.dur_fmt(&t->pos, buf, 10, "mm:ss");
    sprintf(line, "%s. %s", buf, t->title);
    test.streq(line, "00:00. Eternal Sleep");

    t = &c->tracks[i++];
    time.dur_fmt(&t->pos, buf, 10, "mm:ss");
    sprintf(line, "%s. %s", buf, t->title);
    test.streq(line, "04:27. Dreams of an Erotic Salvation");

    t = &c->tracks[i++];
    time.dur_fmt(&t->pos, buf, 10, "mm:ss");
    sprintf(line, "%s. %s", buf, t->title);
    test.streq(line, "08:57. A Mind Confused");
    
    return test.fails();
}
