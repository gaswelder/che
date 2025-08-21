#import clip/vec
#import formats/wav
#import midilib.c

pub int run(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "usage: %s midi-file track-number\n", argv[0]);
		return 1;
	}
	uint8_t track = atoi(argv[2]);
	vec.t *ranges = compose(argv[1], track);
	writeout(ranges);
	free(ranges);
	return 0;
}

typedef {
	uint8_t key;
	size_t t1, t2; // begin and end time in pcm ticks (1/44100 s)
	wav.samplef_t *samples;
	size_t nsamples;
} range_t;

typedef {
	uint8_t key;
	wav.samplef_t *samples;
	size_t nsamples;
} clip_t;

clip_t clips[100] = {};
size_t nclips = 0;

void load_clip(uint8_t key, char *path) {
	if (nclips == nelem(clips)) {
		panic("reached clips limit");
	}
	wav.reader_t *r = wav.open_reader(path);
	if (!r) {
		panic("failed to open %s: %s", path, strerror(errno));
	}
	if (r->wav.frequency != 44100) {
		panic("%s: expected frequency %u, got %u", path, 44100, r->wav.frequency);
	}
	clip_t *c = &clips[nclips++];

	size_t cap = 65536;
	c->samples = calloc!(cap, sizeof(wav.samplef_t));
	size_t n = 0;
	while (wav.more(r)) {
		if (n + 1 > cap) {
			cap *= 2;
			c->samples = realloc(c->samples, cap * sizeof(wav.samplef_t));
			if (!c->samples) {
				panic("realloc failed");
			}
		}
		c->samples[n++] = wav.read_samplef(r);
	}
	c->nsamples = n;
	c->key = key;
	fprintf(stderr, "loaded %u: %zu from %s\n", key, n, path);
}

clip_t *get_clip(uint8_t key) {
	for (size_t i = 0; i < nclips; i++) {
		if (clips[i].key == key) {
			return &clips[i];
		}
	}
	return NULL;
}

vec.t *compose(const char *path, uint8_t track) {
	load_clip(36, "0-36.wav");
	load_clip(40, "0-40.wav");
	load_clip(41, "0-41.wav");
	load_clip(42, "0-42.wav");
	load_clip(43, "0-43.wav");
	load_clip(44, "0-44.wav");
	load_clip(45, "0-45.wav");
	load_clip(46, "0-46.wav");
	load_clip(47, "0-47.wav");
	load_clip(48, "0-48.wav");
	load_clip(49, "0-49.wav");
	load_clip(50, "0-50.wav");
	load_clip(51, "0-51.wav");
	load_clip(53, "0-53.wav");
	load_clip(55, "0-55.wav");
	load_clip(57, "0-57.wav");
	load_clip(59, "0-59.wav");

	size_t n = 0;
	midilib.event_t *ee = NULL;
	midilib.read_file(path, &ee, &n);

	vec.t *ranges = vec.new(sizeof(range_t));

	for (size_t i = 0; i < n; i++) {
		midilib.event_t *e = &ee[i];
		if (e->track != track) continue;
		switch (e->type) {
			case midilib.NOTE_ON: {
				range_t *r = vec.alloc(ranges);
				r->t1 = e->t_us * 44100 / 1000000;
				r->key = e->key;
				clip_t *c = get_clip(e->key);
				if (!c) {
					fprintf(stderr, "no clip for key %u\n", e->key);
				} else {
					r->samples = c->samples;
					r->nsamples = c->nsamples;
				}
			}
			case midilib.NOTE_OFF: {
				// Find the first unclosed matching range.
				range_t *r = NULL;
				size_t nranges = vec.len(ranges);
				for (size_t i = 0; i < nranges; i++) {
					range_t *x = vec.index(ranges, i);
					if (x->key == e->key && x->t2 == 0) {
						r = x;
						break;
					}
				}
				if (!r) {
					panic("failed to find the open range to close");
				}
				r->t2 = e->t_us * 44100 / 1000000;
				size_t tlen = r->t2 - r->t1;
				if (r->nsamples > tlen) {
					r->t2 = r->t1 + r->nsamples;
					// fprintf(stderr, "trimmed sample %u: %zu vs %zu\n", r->key, tlen, r->nsamples);
				}
			}
		}
	}

	free(ee);
	return ranges;
}

void writeout(vec.t *ranges) {
	size_t nranges = vec.len(ranges);
	size_t tmax = 0;
	for (size_t i = 0; i < nranges; i++) {
		range_t *r = vec.index(ranges, i);
		// fprintf(stderr, "%zu: %zu..%zu: %u\n", i, r->t1, r->t2, r->key);
		if (r->t2 > tmax) {
			tmax = r->t2;
		}
	}

	wav.writer_t *out = wav.open_writer(stdout);
	// This is a brute-force scan from 0 to the max time,
	// where for each time instant we query all ranges.
	for (size_t i = 0; i <= tmax; i++) {
		double left = 0;
		double right = 0;
		for (size_t j = 0; j < nranges; j++) {
			range_t *r = vec.index(ranges, j);
			if (i < r->t1 || i >= r->t2) continue;
			size_t pos = i - r->t1;
			if (pos >= r->nsamples) continue;
			left += r->samples[pos].left;
			right += r->samples[pos].right;
		}
		wav.write_sample(out, left, right);
	}
	wav.close_writer(out);
}
