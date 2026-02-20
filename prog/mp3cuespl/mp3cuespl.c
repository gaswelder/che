#import formats/cue
#import formats/mp3
#import os/fs
#import strbuilder

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

	cue.err_t err = {};
	cue.cue_t *c = cue.parse(s, &err);
	if (err.set) {
		fprintf(stderr, "Couldn't parse the cue file: %s\n", err.msg);
		return 1;
	}

	mp3.mp3file *m = mp3.mp3open(mp3path);
	if (!m) {
		fprintf(stderr, "Couldn't open '%s': %s\n", mp3path, strerror(errno));
		return 1;
	}
	cuespl(c, m);
	cue.cue_free(c);
	mp3.mp3close(m);
	return 0;
}

char fname[1000] = {};

void cuespl(cue.cue_t *c, mp3.mp3file *m) {
	int n = cue.cue_ntracks(c);
	for (int i = 0; i < n; i++) {
		// Find the current track's end position in the source file.
		int64_t pos_us = SIZE_MAX;
		if (i+1 < n) {
			pos_us = cue.pos_us(cue.cue_track(c, i+1));
		}

		// Format the file name.
		cue.track_t *track = cue.cue_track(c, i);
		fmtname(fname, sizeof(fname), i, track->title);
		printf("%s\n", fname);

		FILE *out = fopen(fname, "wb");
		if (!out) {
			panic("Couldn't create %s", fname);
		}
		mp3.mp3out(m, out, pos_us);
		fclose(out);
	}
}

void fmtname(char *buf, size_t bufsize, int i, const char *title) {
	strbuilder.str *b = strbuilder.new();
	strbuilder.addf(b, "%02d. ", i+1);
	const char *c = title;
	while (*c != '\0') {
		if (*c == ':') {
			strbuilder.adds(b, " -");
			c++;
			continue;
		}
		if (*c == '/') {
			strbuilder.adds(b, "-");
			c++;
			continue;
		}
		strbuilder.addc(b, *c);
		c++;
	}
	strbuilder.adds(b, ".mp3");

	if (strbuilder.str_len(b) >= bufsize) {
		panic("track title too long");
	}
	strcpy(buf, strbuilder.str_raw(b));
	strbuilder.free(b);
}
