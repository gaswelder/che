#import formats/cue
#import formats/mp3
#import os/fs

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Usage: mp3cuespl <cuefile> <mp3.mp3file>\n");
		return 1;
	}

	const char *cuepath = argv[1];
	const char *mp3path = argv[2];

	char *s = fs.readfile_str(cuepath);
	if (!s) {
		panic("Couldn't read %s", cuepath);
	}

	char *err = NULL;
	cue.cue_t *c = cue.cue_parse(s);
	free(s);
	if (!c) {
		fprintf(stderr, "Couldn't parse the cue file: %s\n", err);
		return 1;
	}

	mp3.mp3file *m = mp3.mp3open(mp3path);
	if (!m) {
		fprintf(stderr, "Couldn't open '%s': %s", mp3path, strerror(errno));
	}
	cuespl(c, m);
	cue.cue_free(c);
	mp3.mp3close(m);
	return 0;
}

void cuespl(cue.cue_t *c, mp3.mp3file *m) {
	int n = cue.cue_ntracks(c);
	for(int i = 0; i < n; i++) {
		cue.track_t *track = cue.cue_track(c, i);

		// Format the track's file name.
		char fname[1000] = {};
		snprintf(fname, sizeof(fname), "%02d. %s.mp3", i+1, track->title);

		// Find the current track's end position in the source file.
		mp3.mp3time_t pos = {0};
		if (i+1 < n) {
			// Go up to the start of the next track.
			pos.usec = cue.pos_us(cue.cue_track(c, i+1));
		} else {
			// If this is the last track, set the position to the end.
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
