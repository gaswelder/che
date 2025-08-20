#import midilib.c
#import formats/wav

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "subcommands: ls, render\n");
		return 1;
	}
    switch str(argv[1]) {
		case "ls": { return main_ls(argc-1, argv+1); }
		case "render": { return main_render(argc-1, argv+1); }
	}
	fprintf(stderr, "subcommands: ls, render\n");
	return 1;
}

int main_ls(int argc, char *argv[]) {
	if (argc != 2) {
        fprintf(stderr, "usage: %s midi-file\n", argv[0]);
        return 1;
    }

    midilib.midi_t *m = midilib.open(argv[1]);
    if (!m) {
        fprintf(stderr, "failed to open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
	size_t n = 0;
	midilib.event_t *ee = NULL;
	midilib.read_file(m, &ee, &n);

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

	wav.samplef_t *s36 = NULL;
	size_t ns36 = 0;
	load_clip("kick-01-mid-1.wav", &s36, &ns36);
	fprintf(stderr, "36: %zu\n", ns36);

	midilib.midi_t *m = midilib.open(argv[1]);
    if (!m) {
        fprintf(stderr, "failed to open %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
	size_t n = 0;
	midilib.event_t *ee = NULL;
	midilib.read_file(m, &ee, &n);

	range_t ranges[1000] = {};
	size_t nranges = 0;

	for (size_t i = 0; i < n; i++) {
		midilib.event_t *e = &ee[i];
		switch (e->type) {
			case midilib.NOTE_ON: {
				range_t *r = &ranges[nranges++];
				r->t1 = e->t_us * 44100 / 1000000;
				r->key = e->key;
				r->samples = s36;
				r->nsamples = ns36;
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

	return 0;
}

void load_clip(char *path, wav.samplef_t **rss, size_t *rn) {
	wav.reader_t *r = wav.open_reader(path);
	if (!r) panic("!");

	size_t cap = 65536;
	wav.samplef_t *samples = calloc!(cap, sizeof(wav.samplef_t));
	size_t n = 0;

	while (wav.more(r)) {
		if (n + 1 > cap) {
			cap *= 2;
			samples = realloc(samples, cap * sizeof(wav.samplef_t));
			if (!samples) {
				panic("realloc failed");
			}
		}
		samples[n++] = wav.read_samplef(r);
	}

	*rss = samples;
	*rn = n;
}
