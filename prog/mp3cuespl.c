import "mp3"
import "cue"
import "fileutil"
import "cli"

int main(int argc, char *argv[])
{
	if(argc != 3) {
		fatal("Usage: mp3cuespl <cuefile> <mp3file>");
	}

	const char *cuepath = argv[1];
	const char *mp3path = argv[2];

	char *s = readfile_str(cuepath);
	if(!s) {
		fatal("Couldn't read %s", cuepath);
	}
	defer free(s);

	cue_t *c = cue_parse(s);
	if(!c) fatal("Couldn't parse");
	defer cue_free(c);

	mp3file *m = mp3open(mp3path);
	if(!m) {
		fatal("Couldn't open '%s'", mp3path);
	}
	defer mp3close(m);

	int n = cue_ntracks(c);
	for(int i = 0; i < n; i++) {
		cuetrack_t *track = cue_track(c, i);

		char fname[1000] = {};
		snprintf(fname, sizeof(fname), "%02d. %s.mp3", i+1, track->title);

		mp3time_t pos = {0};
		if(i+1 < n) {
			cuetrack_t *next = cue_track(c, i+1);
			pos.usec = next->pos_usec;
		}
		else {
			pos.min = -1;
			pos.usec = 0;
		}

		printf("%s\n", fname);
		FILE *out = fopen(fname, "wb");
		if(!out) {
			fatal("Couldn't create %s", fname);
		}
		mp3out(m, out, pos);
		fclose(out);
	}

	return 0;
}
