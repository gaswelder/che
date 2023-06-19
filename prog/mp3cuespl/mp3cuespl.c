#import mp3
#import cue
#import fileutil

int main(int argc, char *argv[])
{
	if (argc != 3) {
		panic("Usage: mp3cuespl <cuefile> <mp3.mp3file>");
	}

	const char *cuepath = argv[1];
	const char *mp3path = argv[2];

	char *s = fileutil.readfile_str(cuepath);
	if (!s) {
		panic("Couldn't read %s", cuepath);
	}

	char *err = NULL;
	cue.cue_t *c = cue.cue_parse(s, &err);
	free(s);
	if (!c) {
		fprintf(stderr, "Couldn't parse the cue file: %s\n", err);
		return 1;
	}

	mp3.mp3file *m = mp3.mp3open(mp3path);
	if(!m) panic("Couldn't open '%s'", mp3path);

	cuespl(c, m);
	cue.cue_free(c);
	mp3.mp3close(m);

	return 0;
}

void cuespl(cue.cue_t *c, mp3.mp3file *m) {
	int n = cue.cue_ntracks(c);
	for(int i = 0; i < n; i++) {
		cue.cuetrack_t *track = cue.cue_track(c, i);

		char fname[1000] = {};
		snprintf(fname, sizeof(fname), "%02d. %s.mp3", i+1, track->title);

		mp3.mp3time_t pos = {0};
		if(i+1 < n) {
			cue.cuetrack_t *next = cue.cue_track(c, i+1);
			pos.usec = next->pos_usec;
		}
		else {
			pos.min = -1;
			pos.usec = 0;
		}

		printf("%s\n", fname);
		FILE *out = fopen(fname, "wb");
		if (!out) {
			panic("Couldn't create %s", fname);
		}
		mp3.mp3out(m, out, pos);
		fclose(out);
	}
}
