#import cue.c
#import time
#import test

const char *s = "FILE \"A Mind Confused - A Mind Confused (1995).mp3\" MP3
TRACK 01 AUDIO
  TITLE \"Eternal Sleep\"
  PERFORMER \"A Mind Confused\"
  INDEX 01 00:00:00
TRACK 02 AUDIO
  TITLE \"Dreams of an Erotic Salvation\"
  PERFORMER \"A Mind Confused\"
  INDEX 01 04:27:00
TRACK 03 AUDIO
  TITLE \"A Mind Confused\"
  PERFORMER \"A Mind Confused\"
  INDEX 01 08:57:00
";

int main() {
    cue.cue_t *c = cue.cue_parse(s);

    int n = cue.cue_ntracks(c);
    test.truth("ntracks", n == 3);

    char buf[10] = {};
    char line[400] = {};
    
    int i = 0;
    cue.track_t *t = NULL;

    t = cue.cue_track(c, i++);
    time.dur_fmt(&t->pos, buf, 10, "mm:ss");
    sprintf(line, "%s. %s", buf, t->title);
    test.streq(line, "00:00. Eternal Sleep");

    t = cue.cue_track(c, i++);
    time.dur_fmt(&t->pos, buf, 10, "mm:ss");
    sprintf(line, "%s. %s", buf, t->title);
    test.streq(line, "04:27. Dreams of an Erotic Salvation");

    t = cue.cue_track(c, i++);
    time.dur_fmt(&t->pos, buf, 10, "mm:ss");
    sprintf(line, "%s. %s", buf, t->title);
    test.streq(line, "08:57. A Mind Confused");
    
    return test.fails();
}
