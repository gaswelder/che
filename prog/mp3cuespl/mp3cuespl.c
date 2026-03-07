#import formats/cue
#import formats/mp3
#import os/fs
#import strbuilder
#import strings
#import time
#import tracklist.c
#import writer

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Arguments: <cue-file or txt-tracklist> <mp3-file>\n");
		return 1;
	}
	const char *cuepath = argv[1];
	const char *mp3path = argv[2];

	//
	// Load the tracklist
	//
	track_t *tracks = NULL;
	int n = 0;
	if (strings.ends_with(cuepath, ".cue")) {
		tracks = loadcue(cuepath, &n);
	} else if (strings.ends_with(cuepath, ".txt")) {
		tracks = loadtxt(cuepath, &n);
	} else {
		fprintf(stderr, "expected a .cue or .txt file path as first argument\n");
		return 1;
	}

	//
	// Load the mp3
	//
	mp3.err_t err1 = {};
	mp3.reader_t *m = mp3.open_reader(mp3path, &err1);
	if (err1.set) {
		fprintf(stderr, "failed to create mp3 reader: %s\n", err1.msg);
		return 1;
	}

	//
	// Split
	//
	cuespl(tracks, n, m);

	free(tracks);
	mp3.close_reader(m);
	return 0;
}

typedef {
	int num;
	char title[1000];
	int64_t pos_us;
} track_t;

// Loads a tracklist from a cue file.
track_t *loadcue(const char *cuepath, int *ret_n) {
	char *s = fs.readfile_str(cuepath);
	if (!s) {
		panic("Couldn't read %s", cuepath);
	}
	cue.err_t err = {};
	cue.cue_t *c = cue.parse(s, &err);
	if (err.set) {
		panic("Couldn't parse the cue file: %s\n", err.msg);
	}
	int n = cue.cue_ntracks(c);
	track_t *tracks = calloc!((size_t) n, sizeof(track_t));
	for (int i = 0; i < n; i++) {
		cue.track_t *track = cue.cue_track(c, i);
		tracks[i].num = i;
		tracks[i].pos_us = cue.pos_us(track);
		strcpy(tracks[i].title, track->title);
	}
	*ret_n = n;
	cue.cue_free(c);
	return tracks;
}

// Loads a tracklist from a text file.
track_t *loadtxt(const char *path, int *ret_n) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		panic("failed to open %s", path);
	}
	track_t *tracks = calloc!(100, sizeof(track_t));
	char line[4096] = {};
	int i = 0;
	int64_t pos_us;
	while (fgets(line, sizeof(line), f)) {
		tracklist.entry_t t = {};
		tracklist.parseline(line, &t);

		track_t *tr = &tracks[i++];
		strcpy(tr->title, t.title);
		tr->num = t.num;
		tr->pos_us = pos_us;
		pos_us += t.duration_us;
	}
	fclose(f);
	*ret_n = i;
	return tracks;
}

char fname[1000] = {};

void cuespl(track_t *tracks, int n, mp3.reader_t *m) {
	for (int i = 0; i < n; i++) {
		size_t pos_us = SIZE_MAX;
		if (i+1 < n) {
			pos_us = tracks[i+1].pos_us;
		}
		fmtname(fname, sizeof(fname), i, tracks[i].title);
		write_track(m, fname, pos_us);
	}
}

void write_track(mp3.reader_t *m, const char *fname, size_t pos_us) {
	FILE *out = fopen(fname, "wb");
	if (!out) {
		panic("Couldn't create %s", fname);
	}

	uint32_t frames = 0;
	uint32_t bytes = 0;

	// Write a dummy info frame
	if (m->infoframelen > 0) {
		for (size_t i = 0; i < m->infoframelen; i++) {
			fputc(m->infoframe[i], out);
		}
		frames = 2; // Dunno why, setting 1 makes the player show 1 second less.
		bytes = m->infoframelen;
	}

	while (m->time < pos_us) {
		mp3.write_frame(m, out);
		frames++;
		bytes += m->framelen;
		if (!mp3.nextframe(m)) {
			break;
		}
	}

	// Get back and patch the info frame
	if (m->infoframelen > 0) {
		writer.t *w = writer.file(out);
		fseek(out, 4 + 32, SEEK_SET);
		mp3.write_xing(w, frames, bytes);
		writer.free(w);
	}

	char buf[20];
	time.duration_t d = time.newdur(frames * 1152 / 44100, time.SECONDS);
	time.dur_fmt(&d, buf, 20, "mm:ss.ms");

	printf("%8u frames (%s)\t%u bytes\t%s\n", frames, buf, bytes, fname);

	fclose(out);
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
