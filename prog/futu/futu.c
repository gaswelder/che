#import formats/wav
#import midilib.c
#import opt

int main(int argc, char *argv[]) {
	opt.addcmd("ls", main_ls);
	opt.addcmd("render", main_render);
	return opt.dispatch(argc, argv);
}

int main_ls(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s midi-file\n", argv[0]);
		return 1;
	}

	size_t n = 0;
	midilib.event_t *ee = NULL;
	midilib.read_file(argv[1], &ee, &n);

	for (size_t i = 0; i < n; i++) {
		midilib.event_t *e = &ee[i];

		printf("%f s\ttrack=%d\t", (double)e->t_us/1e6, e->track);
		switch (e->type) {
			case midilib.END: {
				printf("end of track\n");
			}
			case midilib.NOTE_OFF: {
				printf("note off\tchannel=%d\tnote=%d\tvelocity=%d\n", e->channel, e->key, e->velocity);
			}
			case midilib.NOTE_ON: {
				printf("note on \tchannel=%d\tnote=%d\tvelocity=%d\n", e->channel, e->key, e->velocity);
			}
			case midilib.TEMPO: {
				printf("set tempo\t%u us per quarter note\n", e->val);
			}
			case midilib.TRACK_NAME: {
				printf("track name: \"%s\"\n", e->str);
			}
			default: {
				panic("?");
			}
		}
	}

	free(ee);
	return 0;
}

typedef {
	uint8_t key;
	size_t t1, t2; // begin and end time in pcm ticks (1/44100 s)
	wav.samplef_t *samples;
	size_t nsamples;
} range_t;

int main_render(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s midi-file\n", argv[0]);
		return 1;
	}

	range_t *ranges = NULL;
	size_t nranges = 0;
	compose(argv[1], &ranges, &nranges);
	writeout(ranges, nranges);
	free(ranges);
	return 0;
}

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

void compose(const char *path, range_t **rranges, size_t *rnranges) {
	load_clip(36, "kick-01-mid-1.wav");

	size_t n = 0;
	midilib.event_t *ee = NULL;
	midilib.read_file(path, &ee, &n);

	range_t *ranges = calloc!(1000, sizeof(range_t));
	size_t nranges = 0;

	for (size_t i = 0; i < n; i++) {
		midilib.event_t *e = &ee[i];
		switch (e->type) {
			case midilib.NOTE_ON: {
				if (nranges == 1000) {
					panic("reached 1000 ranges");
				}
				range_t *r = &ranges[nranges++];
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
				for (size_t i = 0; i < nranges; i++) {
					range_t *x = &ranges[i];
					if (x->key == e->key && x->t2 == 0) {
						r = x;
						break;
					}
				}
				if (!r) {
					panic("failed to find the open range to close");
				}
				r->t2 = e->t_us * 44100 / 1000000;
			}
		}
	}

	free(ee);
	*rranges = ranges;
	*rnranges = nranges;
}

void writeout(range_t *ranges, size_t nranges) {
	size_t tmax = 0;
	for (size_t i = 0; i < nranges; i++) {
		range_t *r = &ranges[i];
		fprintf(stderr, "%zu: %zu..%zu: %u\n", i, r->t1, r->t2, r->key);
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
			range_t *r = &ranges[j];
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
